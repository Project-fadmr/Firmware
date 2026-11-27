#ifndef ESPNOW_PROTO_H
#define ESPNOW_PROTO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EP_MAX_FRAME   1024
#define EP_PKT_HDR     3
#define EP_PKT_PAYLOAD 247
#define EP_SYNC0       0xA5
#define EP_SYNC1       0x5A
#define EP_OVERHEAD    13
#define EP_SLOTS       4

typedef struct {
  uint8_t mac[6];
  int8_t rssi;
  uint16_t len;
  const uint8_t *data;
} ep_frame_t;

uint16_t ep_crc16(const uint8_t *d, size_t n);
size_t ep_encode(const uint8_t mac[6], int8_t rssi, const uint8_t *payload,
                 uint16_t len, uint8_t *out, size_t cap);

typedef struct {
  uint8_t buf[EP_MAX_FRAME + EP_OVERHEAD];
  size_t n;
  uint32_t bad;
  size_t pend_off, pend_n; /* unread replay bytes in buf, drained by later feeds */
} ep_parser_t;

void ep_parser_init(ep_parser_t *p);
bool ep_parser_feed(ep_parser_t *p, uint8_t b, ep_frame_t *out);
bool ep_parser_mid_frame(const ep_parser_t *p);

typedef struct {
  bool used;
  uint8_t mac[6];
  uint8_t seq, count;
  uint8_t got_mask;
  uint16_t len;
  uint32_t last_ms;
  uint8_t buf[EP_MAX_FRAME];
} ep_slot_t;

typedef struct {
  ep_slot_t slot[EP_SLOTS];
  uint32_t rejected, discarded;
} ep_reasm_t;

void ep_reasm_init(ep_reasm_t *r);
bool ep_reasm_push(ep_reasm_t *r, const uint8_t mac[6], int8_t rssi,
                   const uint8_t *pkt, size_t len, uint32_t now_ms,
                   ep_frame_t *out);

#ifdef __cplusplus
}
#endif
#endif
