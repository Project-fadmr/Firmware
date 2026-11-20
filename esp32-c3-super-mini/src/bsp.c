//Board Support Program for the ESP32-C3-Super-Mini
//Designed for project faðmr
//Version: 0
//Revised: 20/11/2026 @lilleole, @voxvoltera

/* bsp_super_mini.c - implementation, see bsp_super_mini.h */
#include "bsp_super_mini.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

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
