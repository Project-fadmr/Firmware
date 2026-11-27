//ESP32-C3-Super-Mini app entry
//Designed for Project faðmr
//Version: 1
//Revised: 27/11/2026 @voxvoltera
//
// Bring-up runs as a list of stages. Each stage is retried, and if it still fails the board
// blinks the stage number on the LED (n blinks, pause) and reboots after RESTART_AFTER_FAIL_S.
// The radar stays powered and configured across reboots, so a restart is cheap.
//
// LED while running: on = streaming. While failing: blink count = stage number.

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "bsp.h"
#include "awr1843_drv.h"
#include "espnow_ll.h"

static const char *TAG = "main";

#define HEALTH_PERIOD_MS      10000
#define RESTART_AFTER_FAIL_S  15

// The number is also the blink count on the LED.
typedef enum {
    STAGE_BSP           = 1,
    STAGE_NVS           = 2,
    STAGE_RADAR_UART    = 3,
    STAGE_RADAR_CONNECT = 4,   // 4 blinks = radar CLI does not answer: wiring, switches, baud, firmware
    STAGE_RADAR_CONFIG  = 5,
    STAGE_LINK          = 6,
    STAGE_STREAM        = 7,
} stage_t;

typedef esp_err_t (*step_fn_t)(void);


/* ---------------- failure handling ---------------- */

static void blink_stage(int n)
{
    for (int i = 0; i < n; i++) {
        bsp_led_set(true);
        vTaskDelay(pdMS_TO_TICKS(150));
        bsp_led_set(false);
        vTaskDelay(pdMS_TO_TICKS(250));
    }
    vTaskDelay(pdMS_TO_TICKS(1500));
}

// Never returns: show the failing stage, then reboot.
static void fatal(stage_t stage, const char *name, esp_err_t err)
{
    ESP_LOGE(TAG, "STAGE %d (%s) FAILED: %s. Rebooting in %d s", (int)stage, name, esp_err_to_name(err),
             RESTART_AFTER_FAIL_S);

    const TickType_t until = xTaskGetTickCount() + pdMS_TO_TICKS(RESTART_AFTER_FAIL_S * 1000);
    while ((int32_t)(xTaskGetTickCount() - until) < 0) {
        blink_stage((int)stage);
    }
    esp_restart();
}

static void run_step(stage_t stage, const char *name, step_fn_t fn, int attempts, uint32_t retry_delay_ms)
{
    esp_err_t err = ESP_FAIL;

    for (int i = 1; i <= attempts; i++) {
        err = fn();
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "%s: ok", name);
            return;
        }
        ESP_LOGW(TAG, "%s: %s (attempt %d/%d)", name, esp_err_to_name(err), i, attempts);
        if (i < attempts) {
            vTaskDelay(pdMS_TO_TICKS(retry_delay_ms));
        }
    }
    fatal(stage, name, err);
}


/* ---------------- stages ---------------- */

static esp_err_t stage_bsp(void)
{
    // LED first, so that every later failure can be signalled
    esp_err_t err = bsp_led_init();
    if (err != ESP_OK) return err;
    err = bsp_power_init();
    if (err != ESP_OK) return err;
    err = bsp_sched_init();
    if (err != ESP_OK) return err;
    return bsp_sched_start();
}

static esp_err_t stage_nvs(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS needs erasing");
        err = nvs_flash_erase();
        if (err == ESP_OK) {
            err = nvs_flash_init();
        }
    }
    return err;
}

// Not retried: uart_driver_install fails the second time round, and a failure here is a build/config bug, not a glitch.
static esp_err_t stage_radar_uart(void)
{
    return awr1843_init();
}

static esp_err_t stage_radar_connect(void)
{
    return awr1843_connect();
}

static esp_err_t stage_radar_config(void)
{
    return awr1843_config();
}

// link_layer_init: "if radio_on fails here, retry with link_layer_radio_on. A second init call returns ESP_OK
// without retrying", so calling init again would hide a failure. Try init once, then only the radio.
static esp_err_t stage_link(void)
{
    static bool init_done = false;
    if (!init_done) {
        esp_err_t err = link_layer_init();
        if (err == ESP_OK) {
            init_done = true;
        }
        return err;
    }
    return link_layer_radio_on();
}

static esp_err_t stage_stream(void)
{
    return bsp_radar_stream_start();
}


/* ---------------- housekeeping ---------------- */

static void health_job(void *ctx)
{
    (void)ctx;
    bsp_mem_stats_t m;
    if (bsp_mem_get_stats(&m) == ESP_OK) {
        ESP_LOGI(TAG, "heap: free %u, min free %u, largest block %u",
                 (unsigned)m.free, (unsigned)m.minimum_free, (unsigned)m.largest_block);
    }
}


/* ---------------- entry ---------------- */

void app_main(void)
{
    run_step(STAGE_BSP, "bsp", stage_bsp, 1, 0);
    ESP_LOGI(TAG, "faðmr C3 starting");

    run_step(STAGE_NVS, "nvs", stage_nvs, 1, 0);

    // Radar: UART up, make sure the CLI answers, push the config.
    // Connect gets many attempts: the radar may still be booting when we start.
    run_step(STAGE_RADAR_UART,    "radar uart",    stage_radar_uart,    1, 0);
    run_step(STAGE_RADAR_CONNECT, "radar connect", stage_radar_connect, 8, 1000);
    run_step(STAGE_RADAR_CONFIG,  "radar config",  stage_radar_config,  3, 500);

    // Radio
    run_step(STAGE_LINK, "link layer", stage_link, 3, 500);

    // Go. Only start streaming once everything it depends on is up.
    run_step(STAGE_STREAM, "radar stream", stage_stream, 1, 0);

    if (bsp_sched_add(health_job, NULL, HEALTH_PERIOD_MS) < 0) {
        ESP_LOGW(TAG, "scheduler full, no health log");
    }

    bsp_led_set(true);   // solid = streaming
    ESP_LOGI(TAG, "streaming");
    // app_main may return: the radar and scheduler tasks keep running
}