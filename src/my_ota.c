// tutor: String manipulation functions
#include <string.h>
// tutor: SDK configuration file - contains build-time settings
#include "sdkconfig.h"

// tutor: FreeRTOS kernel and task libraries
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// tutor: Certificate bundle for validating HTTPS connections (security)
#include "esp_crt_bundle.h"
// tutor: HTTPS OTA (Over-The-Air) update library - handles firmware downloading
#include "esp_https_ota.h"
// tutor: Logging library
#include "esp_log.h"
// tutor: System-level functions (like esp_restart)
#include "esp_system.h"

// tutor: Local header files
#include "my_ota.h"
#include "my_led.h"

// tutor: TAG for logging - all OTA logs show "MY_OTA" prefix
static const char *TAG = "MY_OTA";

// tutor: URL to the firmware binary file
// tutor: HTTPS (secure) URL pointing to MinIO object storage
// tutor: When this function is called, it will download the .bin file from this URL
#define OTA_FIRMWARE_URL "https://minio.ravijey.com/firmware/firmware.bin"

// tutor: OTA TASK - Downloads and flashes new firmware from the server
// tutor: This runs as a background task (called from my_mqtt when command received)
void ota_task(void *pvParameter)
{
    // tutor: Debug logging - print the URL address and value for troubleshooting
    ESP_LOGI(TAG, "RAW OTA URL POINTER: %p", OTA_FIRMWARE_URL);
    ESP_LOGI(TAG, "OTA URL STRING: '%s'", OTA_FIRMWARE_URL);

    ESP_LOGI(TAG, "Starting OTA update...");

    // tutor: Configure HTTP client settings for downloading the firmware
    esp_http_client_config_t http_config = {
        .url = OTA_FIRMWARE_URL,                    // tutor: Where to download from
        .timeout_ms = 10000,                        // tutor: Timeout if download takes >10 seconds
        .crt_bundle_attach = esp_crt_bundle_attach, // tutor: Use built-in CA certificates for HTTPS
    };

    // tutor: Configure OTA settings
    esp_https_ota_config_t ota_config = {
        .http_config = &http_config, // tutor: Use the HTTP config we just set up
    };

    // tutor: Execute OTA update
    // tutor: This downloads the firmware binary, verifies it, and writes to flash
    // tutor: Returns ESP_OK if successful, error code otherwise
    esp_err_t ret = esp_https_ota(&ota_config);

    // tutor: Handle the result
    if (ret == ESP_OK)
    {
        // tutor: Update successful - show success LED and reboot
        ESP_LOGI(TAG, "OTA successful. Rebooting...");
        led_success(); // tutor: Light up LED to indicate success
        esp_restart(); // tutor: Reboot the ESP32 (new firmware takes effect)
    }
    else
    {
        // tutor: Update failed - show error LED
        led_error(); // tutor: Light up error LED
        ESP_LOGE(TAG, "OTA failed.");
    }

    // tutor: Delete this task (should never reach here, but good practice)
    // tutor: After esp_restart(), execution doesn't return here
    vTaskDelete(NULL);
}