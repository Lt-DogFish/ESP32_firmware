#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_event.h"

#include "my_mqtt.h"
#include "my_wifi.h"

static const char *TAG = "MY_MQTT";
static esp_mqtt_client_handle_t client_handle = NULL;

extern char device_mac_str[13];
#define FIRMWARE_VERSION "v1.0.0"

// The asynchronous event handler that listens to the broker connection state
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;

    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED - Broker Pipe is Ready.");

        // 1. Fetch the IP Address dynamically
        char ip_str[16];
        get_wifi_ip_string(ip_str, sizeof(ip_str));

        // 2. Construct the exact JSON string your Python operator expects
        char birth_payload[192];
        snprintf(birth_payload, sizeof(birth_payload),
                 "{\"IP\":\"%s\",\"firmware\":\"%s\",\"MacAddress\":\"%s\"}",
                 ip_str, FIRMWARE_VERSION, device_mac_str);

        // 3. Publish directly using the active event client handle
        // Setting msg_id = 0, qos = 1, and retain = 0
        int msg_id = esp_mqtt_client_publish(client, "esp32/birth", birth_payload, 0, 1, 0);
        ESP_LOGI(TAG, "Sent birth message successfully, msg_id=%d", msg_id);

        // 4. (Optional) Subscribe to your GitOps command topic here as well
        char command_topic[64];
        snprintf(command_topic, sizeof(command_topic), "esp32/commands/%s", device_mac_str);
        esp_mqtt_client_subscribe(client, command_topic, 1);
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "Disconnected from broker. Reconnecting automatically...");
        break;

    case MQTT_EVENT_DATA:
        // Triggers instantly when a command falls down from your Python Operator
        ESP_LOGI(TAG, "Incoming Command Received!");
        printf("Topic: %.*s\r\n", event->topic_len, event->topic);
        printf("Payload: %.*s\r\n", event->data_len, event->data);

        // Handle actions here (e.g., checking if data matches "REBOOT")
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "MQTT Event Error detected.");
        break;

    default:
        break;
    }
}

void my_mqtt_init(const char *broker_uri)
{
    // Configure client spec matching modern ESP-IDF structural layouts
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = broker_uri,
    };

    client_handle = esp_mqtt_client_init(&mqtt_cfg);

    // Register the event handler to capture connection loops
    esp_mqtt_client_register_event(client_handle, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);

    // Fire up the background task engine
    esp_mqtt_client_start(client_handle);
}

void my_mqtt_publish(const char *topic, const char *json_string)
{
    if (client_handle != NULL)
    {
        esp_mqtt_client_publish(client_handle, topic, json_string, 0, 0, 0);
    }
    else
    {
        ESP_LOGE(TAG, "Cannot publish. Client not initialized.");
    }
}