//Board Support Program for the ESP32-C3-Super-Mini
//Designed for project faðmr
//Version: 0
//Revised: 20/11/2026 @lilleole, @voxvoltera

 /*
 * RULES
 *   - Scheduler jobs run cooperatively in one task: keep them short, never block.
 *   - Resolution is one FreeRTOS tick (default 10 ms, CONFIG_FREERTOS_HZ).
 */
#ifndef BSP_SUPER_MINI_H
#define BSP_SUPER_MINI_H
 
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "driver/gpio.h"
 
#ifdef __cplusplus
extern "C" {
#endif
 
/* ===================== Configuration ===================== */
 
/* LED */
#define BSP_LED_GPIO              GPIO_NUM_8
#define BSP_LED_ACTIVE_LEVEL      0

/* UART */
#define BSP_UART_RX               GPIO_NUM_20
#define BSP_UART_TX               GPIO_NUM_21
 
/* Scheduler */
#define BSP_SCHED_MAX_JOBS        8
#define BSP_SCHED_TASK_STACK      3072
#define BSP_SCHED_TASK_PRIO       5

/* misc pins */
#define BSP_pin_0                 GPIO_NUM_0
#define BSP_pin_1                 GPIO_NUM_1
#define BSP_pin_2                 GPIO_NUM_2
#define BSP_pin_3                 GPIO_NUM_3
#define BSP_pin_4                 GPIO_NUM_4
#define BSP_pin_5                 GPIO_NUM_5
#define BSP_pin_6                 GPIO_NUM_6
#define BSP_pin_7                 GPIO_NUM_7
#define BSP_pin_9                 GPIO_NUM_9
#define BSP_pin_10                GPIO_NUM_10


/* ===================== API ===================== */
 
/* --- LED --- */
esp_err_t bsp_led_init(void);
void      bsp_led_set(bool on);
void      bsp_led_toggle(void);
 
/* --- Scheduler --- */
typedef void (*bsp_sched_fn_t)(void *ctx);
 
esp_err_t bsp_sched_init(void);
esp_err_t bsp_sched_start(void);
/* Returns job id (>= 0) or -1 if table full / bad args. First run is one period after add. */
int       bsp_sched_add(bsp_sched_fn_t fn, void *ctx, uint32_t period_ms);
void      bsp_sched_remove(int job_id);


/* ---------------- Power Management ---------------- */

esp_err_t bsp_power_init(uint32_t max_freq_mhz,
                         uint32_t min_freq_mhz,
                         bool light_sleep_enable);

esp_err_t bsp_power_lock_acquire(void);
esp_err_t bsp_power_lock_release(void);

esp_err_t bsp_power_deep_sleep(uint64_t sleep_time_us);
esp_err_t bsp_power_init(void);

esp_err_t bsp_power_no_sleep_acquire(void);
esp_err_t bsp_power_no_sleep_release(void);

/* ---------------- Power GPIO ---------------- */

esp_err_t bsp_power_gpio_init(void);


/* ---------------- Memory Management ---------------- */

typedef struct {
    size_t total;
    size_t free;
    size_t minimum_free;
    size_t largest_block;
} bsp_mem_stats_t;

esp_err_t bsp_mem_get_stats(bsp_mem_stats_t *stats);

void *bsp_mem_alloc(size_t size);
void *bsp_mem_calloc(size_t count, size_t size);
void bsp_mem_free(void *ptr);
 
#ifdef __cplusplus
}
#endif
 
#endif /* BSP_SUPER_MINI_H */
