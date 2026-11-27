#ifndef BSP_SCHED_H
#define BSP_SCHED_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BSP_SCHED_MAX_JOBS 8

typedef void (*bsp_sched_fn_t)(void *ctx);

/* Provided by the platform (millis()) or the test's fake clock. */
uint32_t bsp_now_ms(void);

void bsp_sched_init(void);
/* Returns job id, or -1 on NULL fn, zero period or full table. First run at now + period. */
int  bsp_sched_add(bsp_sched_fn_t fn, void *ctx, uint32_t period_ms);
void bsp_sched_remove(int id);
/* Runs each due job once; call from loop(). Jobs may add/remove jobs. */
void bsp_sched_run(void);

#ifdef __cplusplus
}
#endif

#endif
