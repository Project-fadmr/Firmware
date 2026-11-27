#include <Arduino.h>
#include "espnow_drv.h"
#include "bsp.h"

typedef struct {
  uint8_t mac[6];
  int8_t rssi;
  uint16_t len;
  uint8_t data[EP_MAX_FRAME];
} owned_frame_t;

static ep_parser_t parser;
static owned_frame_t fifo[2];
static uint8_t fifo_head, fifo_n;
static bool busy_on;
static uint32_t rx, dropped;

bool espnow_drv_poll(ep_frame_t *out) {
  if (fifo_n) {
    owned_frame_t *f = &fifo[fifo_head];
    memcpy(out->mac, f->mac, 6);
    out->rssi = f->rssi;
    out->len = f->len;
    out->data = f->data;
    fifo_head ^= 1;
    fifo_n--;
    rx++;
    bsp_activity();
    return true;
  }
  while (Serial1.available()) {
    if (ep_parser_feed(&parser, (uint8_t)Serial1.read(), out)) {
      rx++;
      bsp_activity();
      return true;
    }
  }
  return false;
}

void espnow_drv_init(void) {
  Serial1.begin(BSP_NINA_BAUD);
  ep_parser_init(&parser);
  fifo_n = 0;
  bsp_nina_on();
}

void espnow_drv_hold(void) {
  if (!busy_on) {
    busy_on = true;
    bsp_nina_busy(true);
    delayMicroseconds(1000);
  }
  uint32_t start = millis(), last_byte = micros();
  ep_frame_t fr;
  while (millis() - start < 30 && micros() - last_byte < 1000) {
    if (!Serial1.available()) continue;
    last_byte = micros();
    if (!ep_parser_feed(&parser, (uint8_t)Serial1.read(), &fr)) continue;
    if (fifo_n == 2) { dropped++; continue; }
    owned_frame_t *f = &fifo[(fifo_head + fifo_n++) & 1];
    memcpy(f->mac, fr.mac, 6);
    f->rssi = fr.rssi;
    f->len = fr.len;
    memcpy(f->data, fr.data, fr.len);
  }
}

void espnow_drv_release(void) {
  busy_on = false;
  bsp_nina_busy(false);
}

void espnow_drv_stats(espnow_drv_stats_t *s) {
  s->rx = rx;
  s->bad = parser.bad + dropped;
}
