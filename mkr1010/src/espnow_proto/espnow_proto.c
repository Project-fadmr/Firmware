#include "espnow_proto.h"
#include <string.h>

uint16_t ep_crc16(const uint8_t *d, size_t n) {
  uint16_t crc = 0xFFFF;
  while (n--) {
    crc ^= (uint16_t)(*d++ << 8);
    for (int i = 0; i < 8; i++)
      crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
  }
  return crc;
}

size_t ep_encode(const uint8_t mac[6], int8_t rssi, const uint8_t *payload,
                 uint16_t len, uint8_t *out, size_t cap) {
  if (len > EP_MAX_FRAME || cap < (size_t)len + EP_OVERHEAD) return 0;
  out[0] = EP_SYNC0;
  out[1] = EP_SYNC1;
  out[2] = (uint8_t)len;
  out[3] = (uint8_t)(len >> 8);
  memcpy(out + 4, mac, 6);
  out[10] = (uint8_t)rssi;
  if (len) memcpy(out + 11, payload, len);
  uint16_t crc = ep_crc16(out + 2, (size_t)len + 9);
  out[11 + len] = (uint8_t)crc;
  out[12 + len] = (uint8_t)(crc >> 8);
  return (size_t)len + EP_OVERHEAD;
}

void ep_parser_init(ep_parser_t *p) {
  p->n = 0;
  p->bad = 0;
  p->pend_off = p->pend_n = 0;
}

bool ep_parser_mid_frame(const ep_parser_t *p) {
  if (p->n >= 2) return true;
  for (size_t i = 1; i < p->pend_n; i++)
    if (p->buf[p->pend_off + i - 1] == EP_SYNC0 && p->buf[p->pend_off + i] == EP_SYNC1)
      return true;
  return false;
}

enum { ST_MORE, ST_FRAME, ST_REJECT };

/* On ST_REJECT the candidate stays in buf[0..n) for the caller to replay. */
static int step(ep_parser_t *p, uint8_t b, ep_frame_t *out) {
  if (p->n == 0) {
    if (b == EP_SYNC0) p->buf[p->n++] = b;
    return ST_MORE;
  }
  if (p->n == 1) {
    if (b == EP_SYNC1) p->buf[p->n++] = b;
    else if (b != EP_SYNC0) p->n = 0;
    return ST_MORE;
  }
  p->buf[p->n++] = b;
  if (p->n < 4) return ST_MORE;
  size_t len = (size_t)p->buf[2] | ((size_t)p->buf[3] << 8);
  if (len > EP_MAX_FRAME) return ST_REJECT;
  if (p->n < len + EP_OVERHEAD) return ST_MORE;
  uint16_t crc = (uint16_t)(p->buf[p->n - 2] | (p->buf[p->n - 1] << 8));
  if (crc != ep_crc16(p->buf + 2, len + 9)) return ST_REJECT;
  memcpy(out->mac, p->buf + 4, 6);
  out->rssi = (int8_t)p->buf[10];
  out->len = (uint16_t)len;
  out->data = p->buf + 11;
  p->n = 0;
  return ST_FRAME;
}

/* Replays buf[rd..end) through the parser; the parser writes behind rd, so it
   never overwrites unread input. A frame completed mid-replay leaves the
   unread tail pending for the next feed. */
static bool drain(ep_parser_t *p, size_t rd, size_t end, int st, ep_frame_t *out) {
  for (;;) {
    while (st == ST_MORE && rd < end) st = step(p, p->buf[rd++], out);
    if (st == ST_FRAME) {
      p->pend_off = rd;
      p->pend_n = end - rd;
      return true;
    }
    if (st != ST_REJECT) return false;
    p->bad++;
    rd -= p->n - 1;
    memmove(p->buf + rd, p->buf + 1, p->n - 1);
    p->n = 0;
    st = ST_MORE;
  }
}

bool ep_parser_feed(ep_parser_t *p, uint8_t b, ep_frame_t *out) {
  if (p->pend_n) {
    memmove(p->buf, p->buf + p->pend_off, p->pend_n);
    p->buf[p->pend_n] = b;
    size_t end = p->pend_n + 1;
    p->pend_n = 0;
    return drain(p, 0, end, ST_MORE, out);
  }
  int st = step(p, b, out);
  return drain(p, p->n, p->n, st, out);
}

void ep_reasm_init(ep_reasm_t *r) { memset(r, 0, sizeof *r); }

static void slot_start(ep_slot_t *s, const uint8_t mac[6], uint8_t seq, uint8_t count) {
  s->used = true;
  memcpy(s->mac, mac, 6);
  s->seq = seq;
  s->count = count;
  s->got_mask = 0;
  s->len = 0;
}

bool ep_reasm_push(ep_reasm_t *r, const uint8_t mac[6], int8_t rssi,
                   const uint8_t *pkt, size_t len, uint32_t now_ms,
                   ep_frame_t *out) {
  if (len < EP_PKT_HDR) goto reject;
  uint8_t seq = pkt[0], idx = pkt[1], count = pkt[2];
  size_t plen = len - EP_PKT_HDR;
  if (count == 0 || (size_t)(count - 1) * EP_PKT_PAYLOAD >= EP_MAX_FRAME ||
      idx >= count || plen > EP_PKT_PAYLOAD)
    goto reject;
  bool last = idx == count - 1;
  /* a short non-last packet would leave a hole in the reassembled frame */
  if (!last && plen != EP_PKT_PAYLOAD) goto reject;
  size_t off = (size_t)idx * EP_PKT_PAYLOAD;
  if (last && off + plen > EP_MAX_FRAME) goto reject;

  ep_slot_t *s = NULL, *oldest = NULL;
  for (int i = 0; i < EP_SLOTS; i++) {
    ep_slot_t *c = &r->slot[i];
    if (c->used && memcmp(c->mac, mac, 6) == 0) { s = c; break; }
    if (!c->used) { if (!s) s = c; }
    else if (!oldest || (int32_t)(c->last_ms - oldest->last_ms) < 0) oldest = c;
  }
  if (s && s->used) {
    if (s->seq != seq || s->count != count) {
      r->discarded++;
      slot_start(s, mac, seq, count);
    }
  } else {
    if (!s) { s = oldest; r->discarded++; }
    slot_start(s, mac, seq, count);
  }

  s->last_ms = now_ms;
  if (s->got_mask & (1u << idx)) return false;
  memcpy(s->buf + off, pkt + EP_PKT_HDR, plen);
  s->got_mask |= (uint8_t)(1u << idx);
  if (last) s->len = (uint16_t)(off + plen);
  if (s->got_mask != (uint8_t)((1u << count) - 1)) return false;

  memcpy(out->mac, mac, 6);
  out->rssi = rssi;
  out->len = s->len;
  out->data = s->buf;
  s->used = false;
  return true;

reject:
  r->rejected++;
  return false;
}
