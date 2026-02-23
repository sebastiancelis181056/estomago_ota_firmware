#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "driver/gpio.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"

#include "nvs_flash.h"

#include "esp_https_ota.h"
#include "esp_ota_ops.h"
#include "esp_app_desc.h"
#include "esp_crt_bundle.h"

/* ================= CONFIG ================= */

#define WIFI_SSID "Bio"
#define WIFI_PASS "12345678"

#define OTA_URL "https://github.com/sebastiancelis181056/OTA/releases/latest/download/firmware.bin"

#define LED_GPIO GPIO_NUM_2

/* ========================================= */

static const char *TAG = "OTA_LED";

static EventGroupHandle_t wifi_event_group;
#define WIFI_CONNECTED BIT0

/* ================= WIFI ================= */

static void wifi_event_handler(void *arg,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
        esp_wifi_connect();

    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "WiFi desconectado, reintentando...");
        vTaskDelay(pdMS_TO_TICKS(2000));
        esp_wifi_connect();
    }

    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ESP_LOGI(TAG, "WiFi conectado");
        esp_wifi_set_ps(WIFI_PS_NONE);
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED);
    }
}

static void wifi_init(void)
{
    wifi_event_group = xEventGroupCreate();

    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();
}

/* ================= LED ================= */

static void led_task(void *pv)
{
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);

    while (1) {
        gpio_set_level(LED_GPIO, 1);
        ESP_LOGI(TAG, "LED ON");
        vTaskDelay(pdMS_TO_TICKS(2000));

        gpio_set_level(LED_GPIO, 0);
        ESP_LOGI(TAG, "LED OFF");
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

/* ================= OTA ================= */

static void ota_task(void *pv)
{
    xEventGroupWaitBits(wifi_event_group,
                        WIFI_CONNECTED,
                        pdFALSE,
                        pdTRUE,
                        portMAX_DELAY);

    esp_http_client_config_t http_cfg = {
        .url = OTA_URL,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 60000,
        .buffer_size = 8192,
        .buffer_size_tx = 4096,
        .keep_alive_enable = true,
    };

    esp_https_ota_config_t ota_cfg = {
        .http_config = &http_cfg,
    };

    esp_err_t ret = esp_https_ota(&ota_cfg);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "OTA OK, reiniciando...");
        esp_restart();
    }

    ESP_LOGW(TAG, "No hay actualización");
    vTaskDelete(NULL);
}

/* ================= MAIN ================= */

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());

    ESP_LOGI(TAG, "===== APP LED + OTA =====");
    ESP_LOGI(TAG, "Versión: %s",
             esp_app_get_description()->version);

    wifi_init();

    // 🟢 Tu aplicación
    xTaskCreate(led_task, "led_task", 2048, NULL, 5, NULL);

    // 🔵 OTA (siempre igual)
    xTaskCreate(ota_task, "ota_task", 16384, NULL, 5, NULL);

    vTaskDelay(pdMS_TO_TICKS(5000));
    esp_ota_mark_app_valid_cancel_rollback();
}
