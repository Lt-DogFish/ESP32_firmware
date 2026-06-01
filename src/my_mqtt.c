#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_event.h"
#include "my_mqtt.h"

static const char *TAG = "MY_MQTT";
static esp_mqtt_client_handle_t client_handle = NULL;

// The asynchronous event handler that listens to the broker connection state
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) 
{
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Successfully connected to Mosquitto Broker!");
            // Automatically subscribe to your incoming cluster command channel at boot
            esp_mqtt_client_subscribe(client, "esp32/commands/esp32-robot-arm", 0);
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
    if (client_handle != NULL) {
        esp_mqtt_client_publish(client_handle, topic, json_string, 0, 0, 0);
    } else {
        ESP_LOGE(TAG, "Cannot publish. Client not initialized.");
    }
}