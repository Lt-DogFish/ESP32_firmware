#include "my_led.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/rmt.h"

//Onboard LED Control
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
static void ws2812_write(rgb_t *leds, int len)
{
    rmt_item32_t items[len * 24];
    int idx = 0;

    for (int i = 0; i < len; i++)
    {
        uint32_t color = (leds[i].green << 16) | (leds[i].red << 8) | leds[i].blue;

        for (int bit = 23; bit >= 0; bit--)
        {
            if (color & (1 << bit))
            {
                items[idx].level0 = 1;
                items[idx].duration0 = T1H;
                items[idx].level1 = 0;
                items[idx].duration1 = T1L;
            }
            else
            {
                items[idx].level0 = 1;
                items[idx].duration0 = T0H;
                items[idx].level1 = 0;
                items[idx].duration1 = T0L;
            }

            idx++;
        }
    }

    rmt_write_items(RMT_TX_CHANNEL, items, idx, true);
    rmt_wait_tx_done(RMT_TX_CHANNEL, portMAX_DELAY);
}

static void set_color(uint8_t r, uint8_t g, uint8_t b)
{
    rgb_t led;

    led.red = r;
    led.green = g;
    led.blue = b;

    ws2812_write(&led, 1);
}

void led_init(void)
{
    rmt_config_t config =
        RMT_DEFAULT_CONFIG_TX(LED_GPIO, RMT_TX_CHANNEL);

    config.clk_div = 3;

    rmt_config(&config);

    rmt_driver_install(config.channel, 0, 0);

	long_green();
}

void led_off(void)
{
    set_color(0, 0, 0);
}

void blink_green(void)
{
    set_color(20, 100, 100);

    vTaskDelay(pdMS_TO_TICKS(50));

    led_off();
    
    set_color(20, 100, 100);

    vTaskDelay(pdMS_TO_TICKS(50));

    led_off();
    
    set_color(20, 100, 100);

    vTaskDelay(pdMS_TO_TICKS(50));

    led_off();
}

void blink_red(void)
{
    set_color(100, 0, 0);

    vTaskDelay(pdMS_TO_TICKS(50));

    led_off();

    set_color(100, 0, 0);

    vTaskDelay(pdMS_TO_TICKS(50));

    led_off();

    set_color(100, 0, 0);

    vTaskDelay(pdMS_TO_TICKS(50));

    led_off();
}

void long_green(void)
{
    set_color(20, 100, 0);

    vTaskDelay(pdMS_TO_TICKS(1000));

    led_off();
}

void long_red(void)
{
    set_color(100, 0, 0);

    vTaskDelay(pdMS_TO_TICKS(1000));

    led_off();
}

void led_success(void)
{
    blink_green();

    vTaskDelay(pdMS_TO_TICKS(200));

    blink_green();
}

void long_purple(void)
{
	
	set_color(100, 0, 100);

	vTaskDelay(pdMS_TO_TICKS(1000));

	led_off();

}

void led_error(void)
{
    blink_red();

    vTaskDelay(pdMS_TO_TICKS(200));

    blink_red();
}

void glimmer(void){

	set_color(20, 100, 0);
    vTaskDelay(pdMS_TO_TICKS(50));
    led_off();
    vTaskDelay(pdMS_TO_TICKS(25));

	set_color(20, 100, 0);
    vTaskDelay(pdMS_TO_TICKS(50));
    led_off();
    vTaskDelay(pdMS_TO_TICKS(25));

    set_color(20, 100, 0);
    vTaskDelay(pdMS_TO_TICKS(50));
    led_off();
    vTaskDelay(pdMS_TO_TICKS(1800));
    }