// tutor: Standard C string library for string operations
#include <string.h>

// tutor: FreeRTOS kernel and event group libraries for task management and synchronization
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

// tutor: ESP32 event system library for handling WiFi and IP events
#include "esp_event.h"
// tutor: ESP32 logging library for debug output
#include "esp_log.h"
// tutor: ESP32 network interface abstraction layer
#include "esp_netif.h"
// tutor: ESP32 WiFi stack library for WiFi functionality
#include "esp_wifi.h"
// tutor: Non-Volatile Storage (NVS) for persistent data storage (like WiFi credentials)
#include "nvs_flash.h"
// tutor: LwIP TCP/IP stack - provides IP address handling functions
#include "lwip/ip4_addr.h"

// tutor: Local header files for WiFi and LED functionality
#include "my_wifi.h"
#include "my_led.h"

// tutor: WiFi network name (SSID) - the network this ESP32 will connect to
#define WIFI_SSID "ATTEvwkH9e"
// tutor: WiFi password - authentication key for the network
#define WIFI_PASSWORD "t85xxr82m?8h"

// tutor: OPTIMIZATION: Hardcode known WiFi channel to avoid scanning delay
// tutor: Channel 11 is what the AT&T router broadcasts on (reduces connection time)
#define TARGET_WIFI_CHANNEL 11
// tutor: MAC address of the specific router (as bytes). Setting this enables direct connection without scanning.
#define ROUTER_BSSID {0x10, 0xC4, 0xCA, 0x2F, 0x6F, 0xF4}

// tutor: STATIC IP CONFIGURATION - Instead of DHCP, we use a fixed IP for predictability
#define STATIC_IP_ADDR "192.168.1.200" // tutor: ESP32's fixed IP address
#define STATIC_GW_ADDR "192.168.1.254" // tutor: Gateway (router) IP address
#define STATIC_NETMASK "255.255.255.0" // tutor: Subnet mask defines the local network range

// tutor: TAG is a label for ESP_LOG macros (all WiFi logs will show "MY_WIFI" prefix)
static const char *TAG = "MY_WIFI";

// tutor: EVENT GROUP is a synchronization primitive - lets main.c wait until WiFi connects
// tutor: Think of it as a "semaphore" or "flag" that signals when WiFi is ready
static EventGroupHandle_t wifi_event_group;

// tutor: BIT0 is a specific flag in the event group - set when WiFi IP is obtained
#define WIFI_CONNECTED_BIT BIT0

// tutor: EXTERN declarations - these functions are defined in my_led.c
// tutor: We declare them here so we can call them from this file
extern void led_error(void);
extern void long_purple(void);

// tutor: EVENT HANDLER - This function is called automatically by the WiFi driver whenever WiFi events occur
// tutor: It's like an interrupt handler - don't call it directly, the system calls it
static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    // tutor: CASE 1: WiFi hardware just started up
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        ESP_LOGI(TAG, "Wi-Fi Started. Connecting directly via Fast Scan...");
        // tutor: Initiate connection to the WiFi network (uses config we set up in wifi_init)
        esp_wifi_connect();
    }
    // tutor: CASE 2: WiFi disconnected or connection was refused by router
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        ESP_LOGW(TAG, "Disconnected or refused. Backing off before retry...");
        // tutor: Wait 2 seconds before retrying - prevents hammering the router
        // tutor: Some routers temporarily reject rapid reconnection attempts
        vTaskDelay(pdMS_TO_TICKS(2000));
        esp_wifi_connect();
    }
    // tutor: CASE 3: We successfully got an IP address from the router (CONNECTED!)
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        // tutor: Extract the IP info from the event data
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        // tutor: Disable WiFi power saving mode for maximum throughput (we want fast MQTT)
        esp_wifi_set_ps(WIFI_PS_NONE);
        // tutor: SET THE FLAG - this tells main.c that WiFi is ready for MQTT
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

// tutor: UTILITY FUNCTION - Converts the current WiFi IP into a text string
// tutor: Example: converts IP bytes into "192.168.1.200" format
void get_wifi_ip_string(char *ip_buf, size_t buf_len)
{
    // tutor: Get the network interface handle for the WiFi station
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif)
    {
        // tutor: Create a structure to hold IP address info
        esp_netif_ip_info_t ip_info;
        // tutor: Fetch the actual IP info - returns ESP_OK if successful
        if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK)
        {
            // tutor: Format the IP into a readable string (e.g., "192.168.1.200")
            snprintf(ip_buf, buf_len, IPSTR, IP2STR(&ip_info.ip));
            return;
        }
    }
    // tutor: If anything fails, return a placeholder IP
    snprintf(ip_buf, buf_len, "0.0.0.0");
}

// tutor: MAIN INITIALIZATION FUNCTION - Sets up the entire WiFi subsystem
void wifi_init(void)
{
    // tutor: Create the event group object that main.c will wait on
    wifi_event_group = xEventGroupCreate();

    // tutor: STEP 1: Initialize NVS (Non-Volatile Storage)
    // tutor: NVS is like a tiny database on the ESP32's flash memory
    // tutor: It stores WiFi credentials and cached connection info for fast reconnect
    esp_err_t ret = nvs_flash_init();
    // tutor: If NVS has issues, erase it and reinitialize
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    // tutor: If there's still an error, the ESP_ERROR_CHECK macro will crash loudly
    ESP_ERROR_CHECK(ret);

    // tutor: STEP 2: Initialize the network interface layer (abstraction for network operations)
    ESP_ERROR_CHECK(esp_netif_init());
    // tutor: Create the default event loop - this is where all WiFi events are dispatched
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // tutor: STEP 3: Create WiFi station interface (STA mode = client connecting to a router)
    // tutor: As opposed to AP mode where ESP32 acts as a WiFi access point
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();

    // tutor: STEP 4: Configure Static IP (instead of requesting one from DHCP server)
    // tutor: Static IP = predictable address, faster than DHCP negotiation
    // tutor: Stop DHCP client - we're not asking router for an IP
    ESP_ERROR_CHECK(esp_netif_dhcpc_stop(sta_netif));
    // tutor: Create a structure to hold IP, Gateway, and Netmask
    esp_netif_ip_info_t ip_info;
    // tutor: Convert the string "192.168.1.200" into binary IP format
    esp_netif_str_to_ip4(STATIC_IP_ADDR, &ip_info.ip);
    esp_netif_str_to_ip4(STATIC_GW_ADDR, &ip_info.gw);
    esp_netif_str_to_ip4(STATIC_NETMASK, &ip_info.netmask);
    // tutor: Apply the IP configuration to the network interface
    ESP_ERROR_CHECK(esp_netif_set_ip_info(sta_netif, &ip_info));

    // tutor: STEP 5: Initialize WiFi subsystem with default configuration
    // tutor: cfg holds settings like TX power, antenna type, etc.
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // tutor: STEP 6: Register event handlers - connect our callback to WiFi events
    // tutor: When WiFi events happen, the system will call wifi_event_handler()
    // tutor: Subscribe to ALL WiFi events (WIFI_EVENT_ANY_ID = all types)
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    // tutor: Subscribe to IP events (specifically when we get an IP address)
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    // tutor: STEP 7: Build WiFi configuration structure with optimization flags
    // tutor: This tells the WiFi driver HOW to connect and WHAT settings to use
    wifi_config_t wifi_config = {
        .sta = {
            // tutor: Network name (SSID) and password from our #defines
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
            // tutor: Authentication type - WPA2 is the modern secure standard
            .threshold.authmode = WIFI_AUTH_WPA_WPA2_PSK,
            // tutor: Use the hardcoded channel (11) - skips the slow channel scanning
            .channel = TARGET_WIFI_CHANNEL,
            // tutor: Use fast scan instead of full passive scan (major speed improvement)
            .scan_method = WIFI_FAST_SCAN,
            // tutor: Sort by signal strength - connect to strongest nearby AP
            .sort_method = WIFI_CONNECT_AP_BY_SIGNAL,
            // tutor: Listen interval (beacon frames) - 3 is standard
            .listen_interval = 3,
            // tutor: Don't require exact MAC address match (allows roaming if needed)
            .bssid_set = false,

            // tutor: --- SPEED OPTIMIZATIONS (802.11k/v/w standards) ---
            // tutor: 802.11k = Radio Measurement - allows fast roaming between APs
            .rm_enabled = 1,
            // tutor: 802.11v = BSS Transition Management - helps AP suggest faster switches
            .btm_enabled = 1,
            // tutor: Multi-Band Operation - tells router we support 2.4GHz and 5GHz
            .mbo_enabled = 1,
        },
    };

    // tutor: STEP 8: Set WiFi mode to Station (client) mode
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    // tutor: Apply the configuration we built above
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    // tutor: STEP 9: Enable caching - reuse previously stored connection info
    // tutor: PMK = Pairwise Master Key (encryption key cached for fast reconnect)
    // tutor: On first boot this does nothing, but on subsequent boots = much faster!
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));

    // tutor: STEP 10: Start the WiFi hardware - this triggers WIFI_EVENT_STA_START
    // tutor: which will call our event handler and initiate connection
    ESP_ERROR_CHECK(esp_wifi_start());
}

// tutor: ACCESSOR FUNCTION - Lets main.c get the event group to wait on
// tutor: main.c uses this to block execution until WiFi connects
EventGroupHandle_t get_wifi_event_group(void)
{
    return wifi_event_group;
}