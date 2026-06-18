#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_event.h"
#include "mqtt_client.h" // Ensure full client structures are visible
#include "my_mqtt.h"
#include "my_wifi.h"

static const char *TAG = "MY_MQTT";
static esp_mqtt_client_handle_t client_handle = NULL;
static EventGroupHandle_t mqtt_event_group = NULL; // Declared here

extern const char *get_device_mac_str(void);
#define FIRMWARE_VERSION "v1.0.0"

// Linker accessor function implementation
EventGroupHandle_t get_mqtt_event_group(void)
{
    return mqtt_event_group;
}

// The asynchronous event handler that listens to the broker connection state
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;

    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED - Broker Pipe is Ready.");

        // Notify any waiting tasks in main (like telemetry) that the broker is ready
        if (mqtt_event_group != NULL)
        {
            xEventGroupSetBits(mqtt_event_group, MQTT_CONNECTED_BIT);
        }

        // 1. Fetch the IP Address dynamically
        char ip_str[16];
        get_wifi_ip_string(ip_str, sizeof(ip_str));

        // 2. Construct the exact JSON string your Python operator expects
        char birth_payload[192];
        snprintf(birth_payload, sizeof(birth_payload),
                 "{\"IP\":\"%s\",\"firmware\":\"%s\",\"MacAddress\":\"%s\"}",
                 ip_str, FIRMWARE_VERSION, get_device_mac_str());

        // 3. Publish directly using the active event client handle (QoS 1)
        int msg_id = esp_mqtt_client_publish(client, "esp32/birth", birth_payload, 0, 1, 0);
        ESP_LOGI(TAG, "Sent birth message successfully, msg_id=%d", msg_id);

        // 4. Subscribe to GitOps command topic here as well
        char command_topic[64];
        snprintf(command_topic, sizeof(command_topic), "esp32/commands/%s", get_device_mac_str());
        esp_mqtt_client_subscribe(client, command_topic, 1);
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "Disconnected from broker. Reconnecting automatically...");
        if (mqtt_event_group != NULL)
        {
            xEventGroupClearBits(mqtt_event_group, MQTT_CONNECTED_BIT);
        }
        break;

    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "Incoming Command Received!");
        printf("Topic: %.*s\r\n", event->topic_len, event->topic);
        printf("Payload: %.*s\r\n", event->data_len, event->data);
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
    // Initialize the event group object for system tracking before launching client
    if (mqtt_event_group == NULL)
    {
        mqtt_event_group = xEventGroupCreate();
    }

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = broker_uri,
    };

    client_handle = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client_handle, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
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