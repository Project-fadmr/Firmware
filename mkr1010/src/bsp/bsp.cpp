#include "bsp.h"
#include "display.h"

static uint32_t last_activity;
static bool standby;

extern "C" uint32_t bsp_now_ms(void) { return millis(); }

static void standby_job(void *) {
  if (!standby && bsp_now_ms() - last_activity >= BSP_IDLE_TIMEOUT_MS) {
    sm_clear();
    sm_show();
    standby = true;
  }
}

void bsp_init(void) {
  pinMode(NINA_RESETN, OUTPUT);
  digitalWrite(NINA_RESETN, HIGH);
  pinMode(NINA_GPIO0, OUTPUT);
  pinMode(BSP_PIN_NINA_BUSY, OUTPUT);
  digitalWrite(NINA_GPIO0, LOW);
  digitalWrite(BSP_PIN_NINA_BUSY, LOW);
  bsp_sched_init();
  last_activity = bsp_now_ms();
  standby = false;
  bsp_sched_add(standby_job, nullptr, 100);
}

void bsp_nina_on(void) {
  digitalWrite(NINA_GPIO0, HIGH);  // GPIO0 must be HIGH at reset release to boot the app
  digitalWrite(NINA_RESETN, HIGH);
  delay(100);
  digitalWrite(NINA_RESETN, LOW);
}

void bsp_nina_off(void) { digitalWrite(NINA_RESETN, HIGH); }

void bsp_nina_restart(void) {
  bsp_nina_off();
  bsp_nina_on();
}

void bsp_nina_busy(bool on) { digitalWrite(BSP_PIN_NINA_BUSY, on ? HIGH : LOW); }

void bsp_activity(void) {
  last_activity = bsp_now_ms();
  standby = false;
}

bool bsp_in_standby(void) { return standby; }

void bsp_loop(void) {
  bsp_sched_run();
  if (standby) {
    PM->SLEEP.reg = PM_SLEEP_IDLE_CPU;
    __WFI();
  }
}
