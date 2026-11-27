#include <bsp.h>
#include <display.h>
#include <espnow_drv.h>

#define BRIGHTNESS 22

// Processing plugs in here. f->data is only valid during this call.
// Keep it under ~4 ms, or wrap heavy work in espnow_drv_hold()/release().
static void on_frame(const ep_frame_t *f) {
  for (int i = 0; i < 6; i++) {
    if (f->mac[i] < 16) Serial.print('0');
    Serial.print(f->mac[i], HEX);
    Serial.print(i < 5 ? ':' : ' ');
  }
  Serial.print(f->rssi);
  Serial.print(' ');
  Serial.println(f->len);
}

static void frame_job(void *) {
  ep_frame_t f;
  while (espnow_drv_poll(&f)) on_frame(&f);
}

void setup() {
  Serial.begin(115200);
  bsp_init();
  sm_begin(BRIGHTNESS);
  espnow_drv_init();
  bsp_sched_add(frame_job, nullptr, 1);
}

void loop() { bsp_loop(); }
