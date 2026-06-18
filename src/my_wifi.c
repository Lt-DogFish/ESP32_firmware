#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "lwip/ip4_addr.h"

#include "my_wifi.h"
#include "my_led.h"

#define WIFI_SSID "ATTEvwkH9e_2.4"
#define WIFI_PASSWORD "t85xxr82m?8h"

// LIGHTNING-FAST WIFI PROVISIONING CONFIGURATIONS
#define TARGET_WIFI_CHANNEL 11
#define ROUTER_BSSID {0x10, 0xC4, 0xCA, 0x2F, 0x6F, 0xF4}

#define STATIC_IP_ADDR "192.168.1.200"
#define STATIC_GW_ADDR "192.168.1.254"
#define STATIC_NETMASK "255.255.255.0"

static const char *TAG = "MY_WIFI";
static EventGroupHandle_t wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0

extern void led_error(void);
extern void long_purple(void);

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        ESP_LOGI(TAG, "Wi-Fi Started. Connecting directly via Fast Scan...");
        esp_wifi_connect(); // Just connect. Let config handle channel filtering.
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        ESP_LOGW(TAG, "Disconnected or refused. Backing off before retry...");
        // Use a longer backoff to keep the AT&T router from renewing its temporary ban
        vTaskDelay(pdMS_TO_TICKS(2000));
        esp_wifi_connect();
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        esp_wifi_set_ps(WIFI_PS_NONE); // Max throughput
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void get_wifi_ip_string(char *ip_buf, size_t buf_len)
{
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif)
    {
        esp_netif_ip_info_t ip_info;
        if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK)
        {
            snprintf(ip_buf, buf_len, IPSTR, IP2STR(&ip_info.ip));
            return;
        }
    }
    snprintf(ip_buf, buf_len, "0.0.0.0");
}

void wifi_init(void)
{
    wifi_event_group = xEventGroupCreate();

    // 1. NVS MUST be initialized for PMKSA/Flash caching to work!
    // The ESP32 relies on NVS to store past AP profiles and key parameters.
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();

    // Apply Static IP configurations natively...
    ESP_ERROR_CHECK(esp_netif_dhcpc_stop(sta_netif));
    esp_netif_ip_info_t ip_info;
    esp_netif_str_to_ip4(STATIC_IP_ADDR, &ip_info.ip);
    esp_netif_str_to_ip4(STATIC_GW_ADDR, &ip_info.gw);
    esp_netif_str_to_ip4(STATIC_NETMASK, &ip_info.netmask);
    ESP_ERROR_CHECK(esp_netif_set_ip_info(sta_netif, &ip_info));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    // 2. Build the high-speed Roaming & Caching configuration profile
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
            .threshold.authmode = WIFI_AUTH_WPA_WPA2_PSK,
            .channel = TARGET_WIFI_CHANNEL,
            .scan_method = WIFI_FAST_SCAN,
            .sort_method = WIFI_CONNECT_AP_BY_SIGNAL,
            .listen_interval = 3,
            .bssid_set = false,

            // --- 802.11k/v SPEED OPTIMIZATIONS ---
            .rm_enabled = 1,  // Radio Measurement (802.11k) enabled
            .btm_enabled = 1, // BSS Transition Management (802.11v) enabled
            .mbo_enabled = 1, // Multi-Band Operation support (keeps router happy)
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    // 3. FORCE THE CHIP TO USE CACHED VALUES IF PRESENT
    // This tells the driver to check internal storage for existing PMK/BSSID associations
    // before treating this connection like a completely blank slate.
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));

    ESP_ERROR_CHECK(esp_wifi_start());
}

EventGroupHandle_t get_wifi_event_group(void)
{
    return wifi_event_group;
}