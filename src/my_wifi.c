#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#include "my_wifi.h"
#include "my_led.h"

#define WIFI_SSID "ATTEvwkH9e"
#define WIFI_PASSWORD "t85xxr82m?8h"

// LIGHTNING-FAST WIFI PROVISIONING CONFIGURATIONS
#define TARGET_WIFI_CHANNEL 10                            // router's exact 2.4GHz channel here
#define ROUTER_BSSID {0x10, 0xC4, 0xCA, 0x2F, 0x6F, 0xF4} // router's AP MAC address here

#define STATIC_IP_ADDR "192.168.1.200" // <--- Desired static IP for this ESP32
#define STATIC_GW_ADDR "192.168.1.254" // <--- Router Gateway
#define STATIC_NETMASK "255.255.255.0" // <--- Network Mask

static const char *TAG = "MY_WIFI";
static EventGroupHandle_t wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0

static void wifi_event_handler(
    void *arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        // Instantly invoke connection on the configured fast-scan channel
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        ESP_LOGI(TAG, "Retrying WiFi connection...");
        led_error();
        esp_wifi_connect();
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;

        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        long_purple();

        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

// Helper to get the current IP address as a string
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

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Create interface handle pointer so we can alter its DHCP configurations
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();

    // OPTIMIZATION 1: Bypass DHCP negotiation entirely
    esp_netif_ip_info_t ip_info;
    ip_info.ip.addr = esp_ip4addr_aton(STATIC_IP_ADDR);
    ip_info.gw.addr = esp_ip4addr_aton(STATIC_GW_ADDR);
    ip_info.netmask.addr = esp_ip4addr_aton(STATIC_NETMASK);

    // Halt the background DHCP service engine on this interface
    ESP_ERROR_CHECK(esp_netif_dhcpc_stop(sta_netif));
    // Apply static mappings instantly to the hardware abstraction layer
    ESP_ERROR_CHECK(esp_netif_set_ip_info(sta_netif, &ip_info));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT,
        ESP_EVENT_ANY_ID,
        &wifi_event_handler,
        NULL,
        NULL));

    // Keeping this hook active so the event handler still gets called
    // to print the IP string and set the FreeRTOS event bit flags.
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT,
        IP_EVENT_STA_GOT_IP,
        &wifi_event_handler,
        NULL,
        NULL));

    // OPTIMIZATION 2: Pin channel, route BSSID, and alter scan rules
    uint8_t target_bssid[6] = ROUTER_BSSID;

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,

            // Hard bind targets to wipe out the continuous spectrum scan cycle
            .bssid_set = true,
            .bssid = {target_bssid[0], target_bssid[1], target_bssid[2],
                      target_bssid[3], target_bssid[4], target_bssid[5]},
            .channel = TARGET_WIFI_CHANNEL,

            // Tell the driver to aggressively jump straight to the explicit parameters
            .scan_method = WIFI_FAST_SCAN,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}