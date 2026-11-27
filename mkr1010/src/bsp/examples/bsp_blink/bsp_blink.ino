#include <bsp.h>
#include <display.h>

static void blink(void *) {
  digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  bsp_init();
  sm_begin(10);
  bsp_sched_add(blink, nullptr, 500);
}

void loop() { bsp_loop(); }
