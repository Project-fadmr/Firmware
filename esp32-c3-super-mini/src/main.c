//ESP32-C3-Super-Mini app entry
//Designed for Project faðmr
//Version: 0
//Revised: 24/11/2026 @voxvoltera

#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "bsp.h"
#include "awr1843_drv.h"
#include "espnow_ll.h"

static const char *TAG = "main";

static bool step_ok(const char *step, esp_err_t err)
{
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "%s failed: %s", step, esp_err_to_name(err));
        return false;
    }
    return true;
}

void app_main(void)
{
    ESP_ERROR_CHECK(bsp_power_init());
    ESP_ERROR_CHECK(bsp_led_init());
    ESP_ERROR_CHECK(bsp_sched_init());
    ESP_ERROR_CHECK(bsp_sched_start());

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    if (!step_ok("awr1843_init", awr1843_init())) {
        return;
    }
    if (!step_ok("awr1843_config", awr1843_config())) {
        return;
    }
    if (!step_ok("link_layer_init", link_layer_init())) {
        return;
    }
    if (!step_ok("bsp_radar_stream_start", bsp_radar_stream_start())) {
        return;
    }
}
