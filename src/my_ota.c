#include <string.h>
#include "sdkconfig.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_crt_bundle.h"
#include "esp_https_ota.h"
#include "esp_log.h"
#include "esp_system.h"

#include "my_ota.h"
#include "my_led.h"

static const char *TAG = "MY_OTA";

#define OTA_FIRMWARE_URL "https://minio.ravijey.com/firmware/firmware.bin"

void ota_task(void *pvParameter)
{
	ESP_LOGI(TAG, "RAW OTA URL POINTER: %p", OTA_FIRMWARE_URL);
	ESP_LOGI(TAG, "OTA URL STRING: '%s'", OTA_FIRMWARE_URL);

    ESP_LOGI(TAG, "Starting OTA update...");

    esp_http_client_config_t http_config = {
		.url = OTA_FIRMWARE_URL,
		.timeout_ms = 10000,
		.crt_bundle_attach = esp_crt_bundle_attach,
	};

    esp_https_ota_config_t ota_config = {
        .http_config = &http_config,
    };

    esp_err_t ret = esp_https_ota(&ota_config);

    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "OTA successful. Rebooting...");
		led_success();
        esp_restart();
    }
    else
    {
		led_error();
        ESP_LOGE(TAG, "OTA failed.");
    }

    vTaskDelete(NULL);
}