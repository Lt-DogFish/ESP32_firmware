#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "driver/rmt.h"
#include "esp_err.h"
#include "esp_mac.h"
#include "esp_log.h"

#include "my_wifi.h"
#include "my_ota.h"
#include "my_led.h"
#include "my_mqtt.h"

#define FIRMWARE_VERSION "v1.0.0"
static const char *TAG = "MAIN_APP";

// Static allocation for unique string device ID
static char device_mac_str[13];

const char *get_device_mac_str(void)
{
    return device_mac_str;
}

void glimmer_task(void *pvParameters)
{
    while (1)
    {
        glimmer();
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void telemetry_task(void *pvParameters)
{
    char *device_mac = (char *)pvParameters;
    int counter = 0;
    char payload[192];
    char topic[64];

    snprintf(topic, sizeof(topic), "esp32/metrics/%s", device_mac);

    while (1)
    {
        snprintf(payload, sizeof(payload),
                 "{\"device_id\":\"%s\",\"uptime_s\":%d,\"status\":\"OK\"}",
                 device_mac, counter++);

        my_mqtt_publish(topic, payload);

        // Sustained 1-second cadence
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void publish_birth_message(const char *mac, const char *version)
{
    char ip_str[16];
    get_wifi_ip_string(ip_str, sizeof(ip_str));

    char birth_payload[192];

    snprintf(birth_payload, sizeof(birth_payload),
             "{\"IP\":\"%s\",\"firmware\":\"%s\",\"MacAddress\":\"%s\"}",
             ip_str, version, mac);

    ESP_LOGI(TAG, "Publishing birth message to esp32/birth: %s", birth_payload);
    my_mqtt_publish("esp32/birth", birth_payload);
}

void fetch_and_format_mac(void)
{
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(device_mac_str, sizeof(device_mac_str), "%02X%02X%02X%02X%02X%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void app_main(void)
{
    // 1. Initialize local hardware dependencies
    led_init();
    fetch_and_format_mac();
    printf("--- Device Booted. MAC Address: %s ---\n", device_mac_str);

    // 2. Launch background hardware UI tasks
    xTaskCreate(glimmer_task, "glimmer_task", 2048, NULL, 5, NULL);

    // 3. Fire up the network subsystem
    wifi_init();

    // 4. STALL HERE: Prevent downstream network executions until Wi-Fi layer 3 binding completes
    ESP_LOGI(TAG, "Holding application layer until static IP binds...");
    xEventGroupWaitBits(get_wifi_event_group(),
                        WIFI_CONNECTED_BIT,
                        pdFALSE,
                        pdTRUE,
                        portMAX_DELAY);

    // 5. Secure network configuration verified. Safe to initialize MQTT engine
    ESP_LOGI(TAG, "IP binding validated. Initializing MQTT Client Subsystem...");
    my_mqtt_init("mqtt://192.168.1.181:1883");

    // 6. STALL HERE: Wait until MQTT handshake completes successfully
    ESP_LOGI(TAG, "Holding telemetry operations until MQTT broker connection completes...");
    xEventGroupWaitBits(get_mqtt_event_group(),
                        MQTT_CONNECTED_BIT,
                        pdFALSE,
                        pdTRUE,
                        portMAX_DELAY);

    // 7. Pipe is ready. Emit birth message directly to inform Python Kubernetes Operator
    publish_birth_message(device_mac_str, FIRMWARE_VERSION);

    // 8. Spin up active loop tasks safely without any metric packet loss
    ESP_LOGI(TAG, "Launching system telemetry tasks successfully.");
    xTaskCreate(telemetry_task, "telemetry_task", 4096, (void *)device_mac_str, 5, NULL);
}