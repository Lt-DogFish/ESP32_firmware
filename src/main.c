#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/rmt.h"
#include "esp_err.h"

#include "my_wifi.h"
#include "my_ota.h"
#include "my_led.h"




void app_main(void)
{
    led_init();
    wifi_init();
    vTaskDelay(1000);

    xTaskCreate( ota_task, "ota_task", 8192, NULL, 5, NULL);
}