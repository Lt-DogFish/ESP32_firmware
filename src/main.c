#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/rmt.h"
#include "esp_err.h"


//Wifi
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"

#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"



#define RMT_TX_CHANNEL RMT_CHANNEL_0
#define LED_GPIO       48    // Onboard NeoPixel
#define NUM_LEDS       1    // Only 1 onboard LED

typedef struct {
    uint8_t green;
    uint8_t red;
    uint8_t blue;
} rgb_t;

// WS2812 timing in RMT ticks (1 tick = 12.5 ns if clk_div = 1, adjust if needed)
#define T0H  8   // 0.4us
#define T0L  17  // 0.85us
#define T1H  16  // 0.8us
#define T1L  9   // 0.45us

// Convert a single LED to RMT pulses
static void ws2812_write(rmt_channel_t channel, rgb_t *leds, int len) {
    rmt_item32_t items[len * 24];
    int idx = 0;

    for (int i = 0; i < len; i++) {
        uint32_t color = (leds[i].green << 16) | (leds[i].red << 8) | leds[i].blue;
        for (int bit = 23; bit >= 0; bit--) {
            if (color & (1 << bit)) {
                items[idx].level0 = 1;
                items[idx].duration0 = T1H;
                items[idx].level1 = 0;
                items[idx].duration1 = T1L;
            } else {
                items[idx].level0 = 1;
                items[idx].duration0 = T0H;
                items[idx].level1 = 0;
                items[idx].duration1 = T0L;
            }
            idx++;
        }
    }

    rmt_write_items(channel, items, idx, true);
    rmt_wait_tx_done(channel, portMAX_DELAY);
}




static const char *TAG = "HTTPS_TEST";

esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    switch(evt->event_id) {

        case HTTP_EVENT_ON_DATA:
            printf("%.*s", evt->data_len, (char*)evt->data);
            break;

        default:
            break;
    }

    return ESP_OK;
}





void app_main(void)
{
    // Configure RMT for NeoPixel
    rmt_config_t config = RMT_DEFAULT_CONFIG_TX(LED_GPIO, RMT_TX_CHANNEL);
    config.clk_div = 3;  // adjust for timing accuracy
    rmt_config(&config);
    rmt_driver_install(config.channel, 0, 0);

    rgb_t led;
    
    
    ESP_ERROR_CHECK(nvs_flash_init());

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Uses menuconfig WiFi credentials
    ESP_ERROR_CHECK(example_connect());

    ESP_LOGI(TAG, "Connected to WiFi");

    esp_http_client_config_t config = {
        .url = "https://ota.example.com/health",

        // DEVELOPMENT ONLY
        .skip_cert_common_name_check = true,

        // Accept all certs for testing
        .crt_bundle_attach = esp_crt_bundle_attach,

        .event_handler = http_event_handler,
    };

    esp_http_client_handle_t client =
        esp_http_client_init(&config);

    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {

        ESP_LOGI(TAG,
                 "HTTPS Status = %d",
                 esp_http_client_get_status_code(client));

    } else {

        ESP_LOGE(TAG,
                 "HTTPS request failed: %s",
                 esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    
    // while (2) {
    //     led.red = 51; led.green = 15; led.blue = 100;
    //     ws2812_write(RMT_TX_CHANNEL, &led, 1);
    //     vTaskDelay(pdMS_TO_TICKS(50));

    //     led.red = 0; led.green = 0; led.blue = 0;
    //     ws2812_write(RMT_TX_CHANNEL, &led, 1);
    //     vTaskDelay(pdMS_TO_TICKS(25));


    //     led.red = 51; led.green = 15; led.blue = 100;
    //     ws2812_write(RMT_TX_CHANNEL, &led, 1);
    //     vTaskDelay(pdMS_TO_TICKS(50));

    //     led.red = 0; led.green = 0; led.blue = 0;
    //     ws2812_write(RMT_TX_CHANNEL, &led, 1);
    //     vTaskDelay(pdMS_TO_TICKS(25));

    //     led.red = 51; led.green = 15; led.blue = 100;
    //     ws2812_write(RMT_TX_CHANNEL, &led, 1);
    //     vTaskDelay(pdMS_TO_TICKS(50));

    //     // none
    //     led.red = 0; led.green = 0; led.blue = 0;
    //     ws2812_write(RMT_TX_CHANNEL, &led, 1);
    //     vTaskDelay(pdMS_TO_TICKS(1800));
    // }
}