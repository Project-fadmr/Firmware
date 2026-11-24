//Board Support Program for the ESP32-C3-Super-Mini
//Designed for project faðmr
//Version: 0
//Revised: 20/11/2026 @lilleole, @voxvoltera

/* bsp_super_mini.c - implementation, see bsp_super_mini.h */
#include "bsp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <stdlib.h>
#include "esp_pm.h"
#include "esp_sleep.h"
#include "esp_heap_caps.h"
#include "esp_check.h"
#include "esp_pm.h"

/* ---------------- LED ---------------- */

static bool s_bsp_led_on = false;

esp_err_t bsp_led_init(void)
{
    const gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << BSP_LED_GPIO,
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    esp_err_t err = gpio_config(&cfg);
    if (err == ESP_OK) {
        bsp_led_set(false);
    }
    return err;
}

void bsp_led_set(bool on)
{
    s_bsp_led_on = on;
    gpio_set_level(BSP_LED_GPIO, on ? BSP_LED_ACTIVE_LEVEL : !BSP_LED_ACTIVE_LEVEL);
}

void bsp_led_toggle(void)
{
    bsp_led_set(!s_bsp_led_on);
}

/* ---------------- Scheduler ---------------- */

typedef struct {
    bsp_sched_fn_t fn;
    void          *ctx;
    TickType_t     period;
    TickType_t     next;
    bool           used;
} bsp_job_t;

typedef struct {
    bsp_sched_fn_t fn;
    void          *ctx;
} bsp_due_t;

static bsp_job_t         s_bsp_jobs[BSP_SCHED_MAX_JOBS];
static SemaphoreHandle_t s_bsp_sched_lock;
static TaskHandle_t      s_bsp_sched_task;

static void bsp_sched_task_fn(void *arg)
{
    (void)arg;
    bsp_due_t due[BSP_SCHED_MAX_JOBS];

    for (;;) {
        int        nd   = 0;
        TickType_t now  = xTaskGetTickCount();
        TickType_t wait = portMAX_DELAY;

        xSemaphoreTake(s_bsp_sched_lock, portMAX_DELAY);
        for (int i = 0; i < BSP_SCHED_MAX_JOBS; i++) {
            bsp_job_t *j = &s_bsp_jobs[i];
            if (!j->used) continue;

            if ((int32_t)(now - j->next) >= 0) {
                due[nd].fn  = j->fn;
                due[nd].ctx = j->ctx;
                nd++;

                j->next += j->period;
                if ((int32_t)(now - j->next) >= 0) {
                    j->next = now + j->period;      /* fell behind: skip missed runs */
                }
            }

            TickType_t remaining = j->next - now;
            if (remaining < wait) wait = remaining;
        }
        xSemaphoreGive(s_bsp_sched_lock);

        /* Run outside the lock so jobs may call add/remove. */
        for (int i = 0; i < nd; i++) {
            due[i].fn(due[i].ctx);
        }

        /* Sleep until the next job is due, or until a new job is added. */
        ulTaskNotifyTake(pdTRUE, wait);
    }
}

esp_err_t bsp_sched_init(void)
{
    if (s_bsp_sched_lock) return ESP_OK;
    s_bsp_sched_lock = xSemaphoreCreateMutex();
    return s_bsp_sched_lock ? ESP_OK : ESP_ERR_NO_MEM;
}

esp_err_t bsp_sched_start(void)
{
    if (!s_bsp_sched_lock) return ESP_ERR_INVALID_STATE;
    if (s_bsp_sched_task)  return ESP_OK;

    BaseType_t ok = xTaskCreate(bsp_sched_task_fn, "bsp_sched", BSP_SCHED_TASK_STACK,
                                NULL, BSP_SCHED_TASK_PRIO, &s_bsp_sched_task);
    return ok == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}

int bsp_sched_add(bsp_sched_fn_t fn, void *ctx, uint32_t period_ms)
{
    if (!s_bsp_sched_lock || !fn || period_ms == 0) return -1;

    TickType_t period = pdMS_TO_TICKS(period_ms);
    if (period == 0) period = 1;

    int id = -1;
    xSemaphoreTake(s_bsp_sched_lock, portMAX_DELAY);
    for (int i = 0; i < BSP_SCHED_MAX_JOBS; i++) {
        if (!s_bsp_jobs[i].used) {
            s_bsp_jobs[i].fn     = fn;
            s_bsp_jobs[i].ctx    = ctx;
            s_bsp_jobs[i].period = period;
            s_bsp_jobs[i].next   = xTaskGetTickCount() + period;
            s_bsp_jobs[i].used   = true;
            id = i;
            break;
        }
    }
    xSemaphoreGive(s_bsp_sched_lock);

    if (id >= 0 && s_bsp_sched_task) xTaskNotifyGive(s_bsp_sched_task);
    return id;
}

void bsp_sched_remove(int job_id)
{
    if (!s_bsp_sched_lock || job_id < 0 || job_id >= BSP_SCHED_MAX_JOBS) return;

    xSemaphoreTake(s_bsp_sched_lock, portMAX_DELAY);
    s_bsp_jobs[job_id].used = false;
    xSemaphoreGive(s_bsp_sched_lock);
}


/* ---------------- Power Management ---------------- */

static esp_pm_lock_handle_t s_no_sleep_lock;
static bool s_power_initialized = false;

esp_err_t bsp_power_init(void)
{
    if (s_power_initialized) {
        return ESP_OK;
    }

    const esp_pm_config_t config = {
        .max_freq_mhz = 160,
        .min_freq_mhz = 40,
        .light_sleep_enable = true,
    };

    ESP_RETURN_ON_ERROR(
        esp_pm_configure(&config),
        "BSP",
        "Failed to configure power management"
    );

    ESP_RETURN_ON_ERROR(
        esp_pm_lock_create(
            ESP_PM_NO_LIGHT_SLEEP,
            0,
            "bsp_no_sleep",
            &s_no_sleep_lock
        ),
        "BSP",
        "Failed to create power lock"
    );

    s_power_initialized = true;

    return ESP_OK;
}

esp_err_t bsp_power_no_sleep_acquire(void)
{
    if (!s_power_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    return esp_pm_lock_acquire(s_no_sleep_lock);
}

esp_err_t bsp_power_no_sleep_release(void)
{
    if (!s_power_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    return esp_pm_lock_release(s_no_sleep_lock);
}


/* ---------------- Power GPIO ---------------- */


esp_err_t bsp_power_gpio_init(void)
{
    const gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << BSP_UART_TX,
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };

    esp_err_t err = gpio_config(&cfg);
    if (err != ESP_OK) {
        return err;
    }

    // Set GPIO21 LOW.
    err = gpio_set_level(BSP_UART_TX, 0);
    if (err != ESP_OK) {
        return err;
    }

    // Retain GPIO state during light sleep.
    return gpio_hold_en(BSP_UART_TX);
}


/* ---------------- Memory Management ---------------- */

esp_err_t bsp_mem_get_stats(bsp_mem_stats_t *stats)
{
    if (!stats) return ESP_ERR_INVALID_ARG;

    stats->total = heap_caps_get_total_size(MALLOC_CAP_8BIT);

    stats->free = heap_caps_get_free_size(MALLOC_CAP_8BIT);

    stats->minimum_free =
        heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT);

    stats->largest_block =
        heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);

    return ESP_OK;
}

void *bsp_mem_alloc(size_t size)
{
    if (size == 0) return NULL;

    return heap_caps_malloc(size, MALLOC_CAP_8BIT);
}

void *bsp_mem_calloc(size_t count, size_t size)
{
    if (count == 0 || size == 0) return NULL;

    return heap_caps_calloc(count, size, MALLOC_CAP_8BIT);
}

void bsp_mem_free(void *ptr)
{
    if (ptr) {
        heap_caps_free(ptr);
    }
}
