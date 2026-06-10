#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/rmt.h"
#include "esp_err.h"
#include "esp_mac.h"

#include "my_wifi.h"
#include "my_ota.h"
#include "my_led.h"
#include "my_mqtt.h" // Your new clean wrapper

// --- The FreeRTOS Glimmer Task ---
void glimmer_task(void *pvParameters)
{
    // The task must run inside an infinite loop so it doesn't instantly exit
    while (1)
    {
        glimmer();

        // Block the task for exactly 2000 milliseconds (2 seconds)
        // This yields the CPU back to the operating system during idle time
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

// current firmware version for initial testing
#define FIRMWARE_VERSION "v1.0.0"

void telemetry_task(void *pvParameters)
{
    // Cast the parameter to get our unique MAC address string
    char *device_mac = (char *)pvParameters;

    int counter = 0;
    char payload[192];
    char topic[64];

    // Dynamically build the topic path using the MAC address
    // e.g., "esp32/metrics/4C75251A3B44"
    snprintf(topic, sizeof(topic), "esp32/metrics/%s", device_mac);

    while (1)
    {
        // Construct the JSON metrics payload with the dynamic MAC
        snprintf(payload, sizeof(payload),
                 "{\"device_id\":\"%s\",\"uptime_s\":%d,\"status\":\"OK\"}",
                 device_mac, counter++);

        my_mqtt_publish(topic, payload);

        // Delay exactly 1 second
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void publish_birth_message(const char *mac, const char *version)
{
    char ip_str[16];
    get_wifi_ip_string(ip_str, sizeof(ip_str));

    char birth_payload[192];

    // Construct the exact JSON structure your Python operator parses
    snprintf(birth_payload, sizeof(birth_payload),
             "{\"IP\":\"%s\",\"firmware\":\"%s\",\"MacAddress\":\"%s\"}",
             ip_str, version, mac);

    printf("Publishing birth message to esp32/birth: %s\n", birth_payload);

    // Publish to the target topic. Using QoS 1 ensures delivery.
    my_mqtt_publish("esp32/birth", birth_payload);
}

// Helper to get a clean, unique string ID from the chip
// Static allocation ensures the memory remains allocated throughout execution
static char device_mac_str[13];

void fetch_and_format_mac(void)
{
    uint8_t mac[6];
    // Fetch the factory-flashed primary Wi-Fi station MAC address
    esp_read_mac(mac, ESP_MAC_WIFI_STA);

    // Convert bytes into a clean uppercase hexadecimal string
    snprintf(device_mac_str, sizeof(device_mac_str), "%02X%02X%02X%02X%02X%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void app_main(void)
{
    // 1. Initialize local hardware components
    led_init();

    // 2. Fetch the MAC address right away at boot
    fetch_and_format_mac();
    printf("--- Device Booted. MAC Address: %s ---\n", device_mac_str);

    // 3. Launch the Glimmer Task
    // This instantly creates the background thread and assigns it a 2KB stack frame
    xTaskCreate(
        glimmer_task,   // Function pointer to the task code
        "glimmer_task", // Debug name for the FreeRTOS monitor
        2048,           // Stack size in bytes (2KB is plenty for RMT operations)
        NULL,           // Parameter passed into the task (not needed here)
        5,              // Task priority (1-5 is a solid baseline for low-resource tasks)
        NULL            // Task handle pointer (not needed unless deleting the task later)
    );

    // 4. Connect to WiFi and MQTT broker
    wifi_init();
    vTaskDelay(pdMS_TO_TICKS(2000));

    my_mqtt_init("mqtt://192.168.1.181:1883");
    vTaskDelay(pdMS_TO_TICKS(1000)); // Allow MQTT connection to clear

    // 5. Spin up the telemetry loop
    xTaskCreate(telemetry_task, "telemetry_task", 4096, (void *)device_mac_str, 5, NULL);
}