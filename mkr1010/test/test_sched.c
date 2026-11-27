#include <assert.h>
#include <stdio.h>
#include "bsp_sched.h"

static uint32_t g_now;
uint32_t bsp_now_ms(void) { return g_now; }

static int calls;
static void count(void *ctx) { (void)ctx; calls++; }

static void fresh(uint32_t t) { g_now = t; calls = 0; bsp_sched_init(); }

static void test_period(void) {
    fresh(0);
    assert(bsp_sched_add(count, NULL, 10) == 0);
    g_now = 9;  bsp_sched_run(); assert(calls == 0);
    g_now = 10; bsp_sched_run(); assert(calls == 1);
    g_now = 15; bsp_sched_run(); assert(calls == 1);
    g_now = 20; bsp_sched_run(); assert(calls == 2);
}

static void test_add_errors(void) {
    fresh(0);
    assert(bsp_sched_add(NULL, NULL, 10) == -1);
    assert(bsp_sched_add(count, NULL, 0) == -1);
    for (int i = 0; i < BSP_SCHED_MAX_JOBS; i++) assert(bsp_sched_add(count, NULL, 10) == i);
    assert(bsp_sched_add(count, NULL, 10) == -1);
    bsp_sched_remove(3);
    bsp_sched_remove(-1);
    bsp_sched_remove(BSP_SCHED_MAX_JOBS);
    assert(bsp_sched_add(count, NULL, 10) == 3);
}

static int self_id;
static void remove_self(void *ctx) { (void)ctx; calls++; bsp_sched_remove(self_id); }

static void test_remove_self(void) {
    fresh(0);
    self_id = bsp_sched_add(remove_self, NULL, 10);
    g_now = 10; bsp_sched_run();
    g_now = 20; bsp_sched_run();
    g_now = 30; bsp_sched_run();
    assert(calls == 1);
}

static int child_calls;
static void child(void *ctx) { (void)ctx; child_calls++; }
static void parent(void *ctx) { (void)ctx; calls++; if (calls == 1) bsp_sched_add(child, NULL, 10); }

static void test_add_from_job(void) {
    fresh(0); child_calls = 0;
    bsp_sched_add(parent, NULL, 10);
    g_now = 10; bsp_sched_run(); assert(child_calls == 0);
    g_now = 19; bsp_sched_run(); assert(child_calls == 0);
    g_now = 20; bsp_sched_run(); assert(child_calls == 1);
}

static void test_no_burst(void) {
    fresh(0);
    bsp_sched_add(count, NULL, 10);
    g_now = 100; bsp_sched_run(); assert(calls == 1);
    bsp_sched_run(); assert(calls == 1);
    g_now = 109; bsp_sched_run(); assert(calls == 1);
    g_now = 110; bsp_sched_run(); assert(calls == 2);
}

static void test_wraparound(void) {
    fresh(0xFFFFFFF0u);
    bsp_sched_add(count, NULL, 10);
    g_now = 0xFFFFFFFAu; bsp_sched_run(); assert(calls == 1);
    g_now = 0x00000004u; bsp_sched_run(); assert(calls == 2);
}

int main(void) {
    test_period(); test_add_errors(); test_remove_self();
    test_add_from_job(); test_no_burst(); test_wraparound();
    puts("sched ok");
    return 0;
}
