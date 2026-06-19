// tutor: Standard I/O for printf debugging
#include <stdio.h>
// tutor: String manipulation functions
#include <string.h>
// tutor: ESP32 logging library
#include "esp_log.h"
// tutor: ESP32 event system for dispatching MQTT events
#include "esp_event.h"
// tutor: MQTT client library - provides connection and publish/subscribe functionality
#include "mqtt_client.h"
// tutor: Local MQTT header file
#include "my_mqtt.h"
// tutor: WiFi header (we need IP address info)
#include "my_wifi.h"

// tutor: TAG for logging - all MQTT logs show "MY_MQTT" prefix
static const char *TAG = "MY_MQTT";
// tutor: Handle (pointer) to the MQTT client - used to publish/subscribe
// tutor: NULL means client not yet initialized
static esp_mqtt_client_handle_t client_handle = NULL;
// tutor: Event group to signal when MQTT connection is ready
// tutor: Similar to wifi_event_group - used for synchronization
static EventGroupHandle_t mqtt_event_group = NULL;

// tutor: External function from main.c - returns the device MAC address string
extern const char *get_device_mac_str(void);
// tutor: Firmware version constant
#define FIRMWARE_VERSION "v1.0.0"

// tutor: ACCESSOR FUNCTION - main.c calls this to get the MQTT event group
// tutor: main.c waits on this to know when MQTT connection is ready
EventGroupHandle_t get_mqtt_event_group(void)
{
    return mqtt_event_group;
}

// tutor: MQTT EVENT CALLBACK - Called automatically when MQTT events occur
// tutor: Like wifi_event_handler, this is a callback - the MQTT library calls it
// tutor: Don't call this directly - the system does it automatically
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    // tutor: Extract the MQTT event structure from the event data
    esp_mqtt_event_handle_t event = event_data;
    // tutor: Get the client handle from the event
    esp_mqtt_client_handle_t client = event->client;

    // tutor: Handle different types of MQTT events
    switch ((esp_mqtt_event_id_t)event_id)
    {
    // tutor: CASE 1: Successfully connected to MQTT broker
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED - Broker Pipe is Ready.");

        // tutor: Signal any waiting tasks (like main.c) that MQTT is ready
        // tutor: This unblocks the xEventGroupWaitBits in app_main
        if (mqtt_event_group != NULL)
        {
            xEventGroupSetBits(mqtt_event_group, MQTT_CONNECTED_BIT);
        }

        // tutor: === BIRTH MESSAGE ANNOUNCEMENT ===
        // tutor: Get the current WiFi IP address
        char ip_str[16];
        get_wifi_ip_string(ip_str, sizeof(ip_str));

        // tutor: Build the JSON birth payload with device metadata
        char birth_payload[192];
        snprintf(birth_payload, sizeof(birth_payload),
                 "{\"IP\":\"%s\",\"firmware\":\"%s\",\"MacAddress\":\"%s\"}",
                 ip_str, FIRMWARE_VERSION, get_device_mac_str());

        // tutor: Publish birth message using the active broker connection
        // tutor: Parameters: client handle, topic, payload, length(0=strlen), QoS(1), retain flag(0)
        int msg_id = esp_mqtt_client_publish(client, "esp32/birth", birth_payload, 0, 1, 0);
        ESP_LOGI(TAG, "Sent birth message successfully, msg_id=%d", msg_id);

        // tutor: === SUBSCRIBE TO COMMANDS ===
        // tutor: Build device-specific command topic: esp32/commands/10C4CA2F6FF4
        char command_topic[64];
        snprintf(command_topic, sizeof(command_topic), "esp32/commands/%s", get_device_mac_str());
        // tutor: Subscribe to this topic with QoS 1 (at least once delivery)
        esp_mqtt_client_subscribe(client, command_topic, 1);
        break;

    // tutor: CASE 2: Disconnected from MQTT broker
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "Disconnected from broker. Reconnecting automatically...");
        // tutor: Clear the connected bit so anyone waiting knows we're offline
        if (mqtt_event_group != NULL)
        {
            xEventGroupClearBits(mqtt_event_group, MQTT_CONNECTED_BIT);
        }
        break;

    // tutor: CASE 3: Received data from subscribed topic
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "Incoming Command Received!");
        // tutor: Print the topic (limited to event->topic_len to handle non-null-terminated string)
        printf("Topic: %.*s\r\n", event->topic_len, event->topic);
        // tutor: Print the payload data
        printf("Payload: %.*s\r\n", event->data_len, event->data);
        // tutor: TODO: Parse the payload and execute commands
        break;

    // tutor: CASE 4: MQTT error occurred
    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "MQTT Event Error detected.");
        break;

    // tutor: Ignore other event types
    default:
        break;
    }
}

// tutor: INITIALIZATION FUNCTION - Sets up MQTT client and connects to broker
void my_mqtt_init(const char *broker_uri)
{
    // tutor: Create event group BEFORE starting client (needed in event handler)
    if (mqtt_event_group == NULL)
    {
        mqtt_event_group = xEventGroupCreate();
    }

    // tutor: Build MQTT client configuration
    // tutor: This tells the client where the broker is and other settings
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = broker_uri, // tutor: URI example: mqtt://192.168.1.181:1883
    };

    // tutor: Initialize MQTT client with configuration
    // tutor: This creates the client but doesn't connect yet
    client_handle = esp_mqtt_client_init(&mqtt_cfg);

    // tutor: Register our event handler to receive MQTT events
    // tutor: ESP_EVENT_ANY_ID means handle all event types
    esp_mqtt_client_register_event(client_handle, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);

    // tutor: Actually start the MQTT client
    // tutor: This begins the connection process to the broker
    esp_mqtt_client_start(client_handle);
}

// tutor: PUBLISH FUNCTION - Sends JSON data to MQTT broker on a topic
// tutor: This is called by telemetry_task and publish_birth_message
void my_mqtt_publish(const char *topic, const char *json_string)
{
    // tutor: Check if client is initialized before publishing
    if (client_handle != NULL)
    {
        // tutor: Publish to the topic
        // tutor: Parameters: handle, topic, payload string, length(0=strlen), QoS(0=no guarantee), retain(0=don't keep)
        esp_mqtt_client_publish(client_handle, topic, json_string, 0, 0, 0);
    }
    else
    {
        // tutor: If client isn't ready, log an error (this shouldn't happen if synchronization is correct)
        ESP_LOGE(TAG, "Cannot publish. Client not initialized.");
    }
}