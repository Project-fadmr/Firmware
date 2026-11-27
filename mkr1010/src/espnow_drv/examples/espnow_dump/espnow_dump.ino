#include <bsp.h>
#include <display.h>
#include <espnow_drv.h>

static void drain(void *) {
  ep_frame_t f;
  while (espnow_drv_poll(&f)) {
    for (int i = 0; i < 6; i++) {
      if (f.mac[i] < 16) Serial.print('0');
      Serial.print(f.mac[i], HEX);
      Serial.print(i < 5 ? ':' : ' ');
    }
    Serial.print(f.rssi);
    Serial.print(' ');
    Serial.println(f.len);
  }
}

static void stats(void *) {
  espnow_drv_stats_t s;
  espnow_drv_stats(&s);
  Serial.print("rx ");
  Serial.print(s.rx);
  Serial.print(" bad ");
  Serial.println(s.bad);
}

void setup() {
  Serial.begin(115200);
  bsp_init();
  sm_begin(10);
  espnow_drv_init();
  bsp_sched_add(drain, nullptr, 1);
  bsp_sched_add(stats, nullptr, 1000);
}

void loop() { bsp_loop(); }
