// tutor: Standard I/O library for printf
#include <stdio.h>
// tutor: FreeRTOS kernel and task/event group libraries
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
// tutor: Driver for RMT (Remote Control) peripheral (used for WS2812B LED control)
#include "driver/rmt.h"
// tutor: ESP32 error code definitions and macros
#include "esp_err.h"
// tutor: Functions to read MAC addresses and unique device identifiers
#include "esp_mac.h"
// tutor: Logging library
#include "esp_log.h"

// tutor: Application-specific headers
#include "my_wifi.h"
#include "my_ota.h"
#include "my_led.h"
#include "my_mqtt.h"

// tutor: Firmware version string - shown in birth message sent to MQTT
#define FIRMWARE_VERSION "v1.0.0"
// tutor: TAG for log messages - all main.c logs show "MAIN_APP" prefix
static const char *TAG = "MAIN_APP";

// tutor: Buffer to store the device's MAC address as a 12-character string
// tutor: Example: "10C4CA2F6FF4" (6 bytes × 2 hex digits each)
static char device_mac_str[13];

// tutor: GETTER FUNCTION - Returns the device's MAC address string
// tutor: Other modules (like MQTT) call this to get the unique device ID
const char *get_device_mac_str(void)
{
    return device_mac_str;
}

// tutor: BACKGROUND TASK - Runs continuously, making the LED "glimmer" (gentle blink)
// tutor: This creates a visual indicator that the device is running
void glimmer_task(void *pvParameters)
{
    // tutor: Infinite loop - FreeRTOS tasks should never exit
    while (1)
    {
        // tutor: Call the LED glimmer function
        glimmer();
        // tutor: Sleep for 5 seconds before glimmering again
        // tutor: pdMS_TO_TICKS converts milliseconds to FreeRTOS "ticks"
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

// tutor: BACKGROUND TASK - Sends telemetry data (uptime and status) to MQTT every second
// tutor: This is the continuous heartbeat that tells your Python operator the device is alive
void telemetry_task(void *pvParameters)
{
    // tutor: The parameter is actually a pointer to the MAC address string
    char *device_mac = (char *)pvParameters;
    // tutor: Counter increments each cycle to represent uptime
    int counter = 0;
    // tutor: Buffers for the JSON payload and MQTT topic
    char payload[192];
    char topic[64];

    // tutor: Build the MQTT topic using the device MAC
    // tutor: Example: "esp32/metrics/10C4CA2F6FF4"
    snprintf(topic, sizeof(topic), "esp32/metrics/%s", device_mac);

    // tutor: Infinite loop - this task runs for the lifetime of the device
    while (1)
    {
        // tutor: Format a JSON message with device ID, uptime, and status
        // tutor: counter represents seconds of uptime
        snprintf(payload, sizeof(payload),
                 "{\"device_id\":\"%s\",\"uptime_s\":%d,\"status\":\"OK\"}",
                 device_mac, counter++);

        // tutor: Publish the JSON payload to the MQTT broker on the topic
        my_mqtt_publish(topic, payload);

        // tutor: Sleep for exactly 1 second before publishing again
        // tutor: This maintains a consistent 1 Hz telemetry cadence
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// tutor: BIRTH MESSAGE - Announces the device to the Python operator with metadata
// tutor: The operator watches esp32/birth topic and registers new devices
void publish_birth_message(const char *mac, const char *version)
{
    // tutor: Get the current WiFi IP address as a string
    char ip_str[16];
    get_wifi_ip_string(ip_str, sizeof(ip_str));

    // tutor: Buffer to hold the JSON birth payload
    char birth_payload[192];

    // tutor: Format JSON with IP, firmware version, and MAC address
    snprintf(birth_payload, sizeof(birth_payload),
             "{\"IP\":\"%s\",\"firmware\":\"%s\",\"MacAddress\":\"%s\"}",
             ip_str, version, mac);

    // tutor: Log the birth message so we can see it in the serial monitor
    ESP_LOGI(TAG, "Publishing birth message to esp32/birth: %s", birth_payload);
    // tutor: Publish to the birth topic - operator will see this and add device to cluster
    my_mqtt_publish("esp32/birth", birth_payload);
}

// tutor: UTILITY FUNCTION - Reads the ESP32's unique MAC address and formats it as a 12-char string
void fetch_and_format_mac(void)
{
    // tutor: Buffer for 6 bytes of MAC address (WiFi STA = Station mode MAC)
    uint8_t mac[6];
    // tutor: Read the WiFi station MAC from the chip's eFuse (read-only memory)
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    // tutor: Convert 6 bytes into 12 hex characters and store in global device_mac_str
    // tutor: %02X = 2-digit uppercase hex (01X would be lowercase)
    snprintf(device_mac_str, sizeof(device_mac_str), "%02X%02X%02X%02X%02X%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

// tutor: APPLICATION ENTRY POINT - Called by ESP32 bootloader after hardware init
// tutor: This is where our program starts - think of it like main() in a normal C program
void app_main(void)
{
    // tutor: === PHASE 1: LOCAL HARDWARE INITIALIZATION ===
    // tutor: Initialize LED controller (before we use LEDs for status)
    led_init();
    // tutor: Read and format the MAC address into device_mac_str
    fetch_and_format_mac();
    // tutor: Print to console/serial monitor for debugging
    printf("--- Device Booted. MAC Address: %s ---\n", device_mac_str);

    // tutor: === PHASE 2: BACKGROUND TASKS ===
    // tutor: Launch the glimmer task with 2KB stack, priority 5, no parameter
    // tutor: The LED will now blink gently in the background
    xTaskCreate(glimmer_task, "glimmer_task", 2048, NULL, 5, NULL);

    // tutor: === PHASE 3: NETWORK INITIALIZATION ===
    // tutor: Fire up WiFi subsystem (this doesn't block - just starts the process)
    wifi_init();

    // tutor: === PHASE 4: SYNCHRONIZATION POINT - WAIT FOR WIFI ===
    // tutor: Block (stop) execution here until WiFi has connected and assigned an IP
    // tutor: The event group is set by wifi_event_handler when IP is obtained
    // tutor: This prevents MQTT from starting before we have network connectivity
    ESP_LOGI(TAG, "Holding application layer until static IP binds...");
    xEventGroupWaitBits(get_wifi_event_group(),
                        WIFI_CONNECTED_BIT,
                        pdFALSE,        // tutor: Don't clear the bit after reading
                        pdTRUE,         // tutor: Wait for ALL requested bits (only 1 in this case)
                        portMAX_DELAY); // tutor: Wait forever (no timeout)

    // tutor: === PHASE 5: MQTT INITIALIZATION ===
    // tutor: WiFi is now ready, so initialize MQTT client
    // tutor: Broker URI: IP 192.168.1.181 on standard MQTT port 1883 (unencrypted)
    ESP_LOGI(TAG, "IP binding validated. Initializing MQTT Client Subsystem...");
    my_mqtt_init("mqtt://192.168.1.181:1883");

    // tutor: === PHASE 6: SYNCHRONIZATION POINT - WAIT FOR MQTT ===
    // tutor: Block execution until MQTT connection handshake completes
    // tutor: This prevents publishing telemetry before broker connection
    ESP_LOGI(TAG, "Holding telemetry operations until MQTT broker connection completes...");
    xEventGroupWaitBits(get_mqtt_event_group(),
                        MQTT_CONNECTED_BIT,
                        pdFALSE,
                        pdTRUE,
                        portMAX_DELAY);

    // tutor: === PHASE 7: ANNOUNCEMENT ===
    // tutor: Now that MQTT is connected, announce this device to the Python operator
    // tutor: The operator watches esp32/birth and adds new devices to cluster management
    publish_birth_message(device_mac_str, FIRMWARE_VERSION);

    // tutor: === PHASE 8: PRODUCTION TASKS ===
    // tutor: All prerequisites are met, launch the telemetry task
    // tutor: This task will send status updates to MQTT continuously
    // tutor: Stack size: 4KB (more than glimmer because of JSON formatting)
    // tutor: Priority: 5 (same as glimmer, will share CPU time)
    // tutor: Parameter: pointer to device MAC string (telemetry_task will use it)
    ESP_LOGI(TAG, "Launching system telemetry tasks successfully.");
    xTaskCreate(telemetry_task, "telemetry_task", 4096, (void *)device_mac_str, 5, NULL);

    // tutor: NOTE: app_main() doesn't return - the task scheduler takes over
    // tutor: The glimmer and telemetry tasks now run continuously in the background
}