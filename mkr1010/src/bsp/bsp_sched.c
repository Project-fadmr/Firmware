#include "bsp_sched.h"

typedef struct {
    bsp_sched_fn_t fn;
    void *ctx;
    uint32_t period;
    uint32_t next;
} job_t;

static job_t jobs[BSP_SCHED_MAX_JOBS];

void bsp_sched_init(void) {
    for (int i = 0; i < BSP_SCHED_MAX_JOBS; i++) jobs[i].fn = 0;
}

int bsp_sched_add(bsp_sched_fn_t fn, void *ctx, uint32_t period_ms) {
    if (!fn || period_ms == 0) return -1;
    for (int i = 0; i < BSP_SCHED_MAX_JOBS; i++) {
        if (!jobs[i].fn) {
            jobs[i] = (job_t){fn, ctx, period_ms, bsp_now_ms() + period_ms};
            return i;
        }
    }
    return -1;
}

void bsp_sched_remove(int id) {
    if (id >= 0 && id < BSP_SCHED_MAX_JOBS) jobs[id].fn = 0;
}

void bsp_sched_run(void) {
    uint32_t now = bsp_now_ms();
    for (int i = 0; i < BSP_SCHED_MAX_JOBS; i++) {
        job_t *j = &jobs[i];
        if (!j->fn || (int32_t)(now - j->next) < 0) continue;
        uint32_t next = j->next + j->period;
        j->next = ((int32_t)(now - next) >= 0) ? now + j->period : next;
        /* Reschedule before the call: the job may remove itself or reuse its slot. */
        bsp_sched_fn_t fn = j->fn;
        void *ctx = j->ctx;
        fn(ctx);
    }
}
