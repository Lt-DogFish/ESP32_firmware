#ifndef MY_MQTT_H
#define MY_MQTT_H

#include "mqtt_client.h"

/**
 * @brief Initializes and starts the native ESP-IDF MQTT client.
 * @param broker_uri The routing URI of your broker (e.g., "mqtt://192.168.1.50:1883")
 */
void my_mqtt_init(const char *broker_uri);

/**
 * @brief Helper function to publish a telemetry string to the cluster.
 * @param topic The target MQTT topic
 * @param json_string The raw JSON or text payload
 */
void my_mqtt_publish(const char *topic, const char *json_string);

#endif // MY_MQTT_H