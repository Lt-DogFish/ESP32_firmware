#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/rmt.h"
#include "esp_err.h"

#include "my_wifi.h"
#include "my_ota.h"
#include "my_led.h"
#include "my_mqtt.h" // Your new clean wrapper



// --- The FreeRTOS Glimmer Task ---
void glimmer_task(void *pvParameters)
{
    // The task must run inside an infinite loop so it doesn't instantly exit
    while (1) {
        glimmer();
        
        // Block the task for exactly 2000 milliseconds (2 seconds)
        // This yields the CPU back to the operating system during idle time
        vTaskDelay(pdMS_TO_TICKS(1800));
    }
}


void app_main(void)
{
    // 1. Initialize local hardware components
    led_init();
    // 2. Launch the Glimmer Task
    // This instantly creates the background thread and assigns it a 2KB stack frame
    xTaskCreate(
        glimmer_task,     // Function pointer to the task code
        "glimmer_task",   // Debug name for the FreeRTOS monitor
        2048,             // Stack size in bytes (2KB is plenty for RMT operations)
        NULL,             // Parameter passed into the task (not needed here)
        5,                // Task priority (1-5 is a solid baseline for low-resource tasks)
        NULL              // Task handle pointer (not needed unless deleting the task later)
    );

    // 2. Connect to WiFi and MQTT broker
    wifi_init(); 
    vTaskDelay(pdMS_TO_TICKS(2000)); 
    my_mqtt_init("mqtt://192.168.1.181:1883"); 

    // 3. Hand off control to your asynchronous workloads
    // xTaskCreate(ota_task, "ota_task", 8192, NULL, 5, NULL);
}