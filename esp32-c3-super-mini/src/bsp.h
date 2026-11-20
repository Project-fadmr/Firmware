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
#define BSP_UAER_RX               16
#define BSP_UAER_TX               17
 
/* Scheduler */
#define BSP_SCHED_MAX_JOBS        8
#define BSP_SCHED_TASK_STACK      3072
#define BSP_SCHED_TASK_PRIO       5
 
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
 
#ifdef __cplusplus
}
#endif
 
#endif /* BSP_SUPER_MINI_H */
