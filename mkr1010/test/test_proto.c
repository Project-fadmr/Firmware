#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "espnow_proto.h"

static const uint8_t MAC[6] = {1, 2, 3, 4, 5, 6};

static void fill(uint8_t *p, size_t n, uint8_t seed) {
  for (size_t i = 0; i < n; i++) p[i] = (uint8_t)(seed + i * 7);
}

/* Feeds all bytes; returns number of frames, last one in *out. */
static int feed_all(ep_parser_t *p, const uint8_t *b, size_t n, ep_frame_t *out) {
  int frames = 0;
  for (size_t i = 0; i < n; i++)
    if (ep_parser_feed(p, b[i], out)) frames++;
  return frames;
}

static void check_frame(const ep_frame_t *f, const uint8_t mac[6], int8_t rssi,
                        const uint8_t *pl, uint16_t len) {
  assert(memcmp(f->mac, mac, 6) == 0);
  assert(f->rssi == rssi);
  assert(f->len == len);
  assert(memcmp(f->data, pl, len) == 0);
}

static void test_crc_check(void) {
  assert(ep_crc16((const uint8_t *)"123456789", 9) == 0x29B1);
}

static void roundtrip(uint16_t len) {
  uint8_t pl[EP_MAX_FRAME], wire[EP_MAX_FRAME + EP_OVERHEAD];
  ep_parser_t p; ep_frame_t f;
  fill(pl, len, 3);
  size_t n = ep_encode(MAC, -40, pl, len, wire, sizeof wire);
  assert(n == (size_t)len + EP_OVERHEAD);
  ep_parser_init(&p);
  for (size_t i = 0; i + 1 < n; i++) assert(!ep_parser_feed(&p, wire[i], &f));
  assert(ep_parser_feed(&p, wire[n - 1], &f));
  check_frame(&f, MAC, -40, pl, len);
}

static void test_roundtrip_small(void) { roundtrip(10); }

static void test_roundtrip_max(void) {
  uint8_t pl[EP_MAX_FRAME + 1], wire[EP_MAX_FRAME + EP_OVERHEAD + 8];
  roundtrip(1024);
  assert(ep_encode(MAC, 0, pl, 1025, wire, sizeof wire) == 0);
  assert(ep_encode(MAC, 0, pl, 10, wire, 22) == 0);
  assert(ep_encode(MAC, 0, pl, 10, wire, 23) == 23);
}

static void test_garbage_prefix(void) {
  uint8_t junk[200], pl[20], wire[40], all[260];
  ep_parser_t p; ep_frame_t f;
  memset(junk, 'x', sizeof junk);
  memcpy(junk, "ets Jun  8 2016 rst:0x1 ", 24);
  junk[100] = 0xA5; junk[101] = 0x5A;
  fill(pl, 20, 9);
  size_t n = ep_encode(MAC, -7, pl, 20, wire, sizeof wire);
  memcpy(all, junk, 200); memcpy(all + 200, wire, n);
  ep_parser_init(&p);
  assert(feed_all(&p, all, 200 + n, &f) == 1);
  check_frame(&f, MAC, -7, pl, 20);
  assert(p.bad >= 1);
}

static void test_bad_crc(void) {
  uint8_t pl[16], wire[40];
  ep_parser_t p; ep_frame_t f;
  fill(pl, 16, 1);
  size_t n = ep_encode(MAC, -1, pl, 16, wire, sizeof wire);
  ep_parser_init(&p);
  wire[12] ^= 0x01;
  assert(feed_all(&p, wire, n, &f) == 0);
  assert(p.bad == 1);
  wire[12] ^= 0x01;
  assert(feed_all(&p, wire, n, &f) == 1);
  check_frame(&f, MAC, -1, pl, 16);
}

static void test_false_sync_in_payload(void) {
  uint8_t pl[20], wire[40], all[80];
  ep_parser_t p; ep_frame_t f;
  fill(pl, 20, 5);
  pl[4] = 0xA5; pl[5] = 0x5A;
  size_t n = ep_encode(MAC, -9, pl, 20, wire, sizeof wire);
  /* truncated frame claiming len 10: sync + header, then cut off */
  const uint8_t trunc[11] = {0xA5, 0x5A, 10, 0, 1, 2, 3, 4, 5, 6, 0xD8};
  memcpy(all, trunc, 11); memcpy(all + 11, wire, n);
  ep_parser_init(&p);
  assert(feed_all(&p, all, 11 + n, &f) == 1);
  check_frame(&f, MAC, -9, pl, 20);
}

static void test_nested_reject(void) {
  uint8_t pl[30], wire[60], all[120];
  ep_parser_t p; ep_frame_t f;
  fill(pl, 30, 5);
  size_t n = ep_encode(MAC, -9, pl, 30, wire, sizeof wire);
  uint8_t bad1[11] = {0xA5, 0x5A, 40, 0, 1, 2, 3, 4, 5, 6, 0};
  uint8_t bad2[7] = {0xA5, 0x5A, 0, 9, 0xA5, 0x5A, 8};
  memcpy(all, bad1, 11); memcpy(all + 11, bad2, 7); memcpy(all + 18, wire, n);
  ep_parser_init(&p);
  assert(feed_all(&p, all, 18 + n, &f) == 1);
  check_frame(&f, MAC, -9, pl, 30);
  assert(p.bad >= 2);
}

static void test_swallowed_frames(void) {
  uint8_t pa[10], pb[10], pc[10], fa[23], fb[23], fc[23], all[11 + 23 + 23 + 1 + 23];
  ep_parser_t p; ep_frame_t f;
  fill(pa, 10, 1); fill(pb, 10, 2); fill(pc, 10, 3);
  assert(ep_encode(MAC, -1, pa, 10, fa, 23) == 23);
  assert(ep_encode(MAC, -2, pb, 10, fb, 23) == 23);
  assert(ep_encode(MAC, -3, pc, 10, fc, 23) == 23);
  /* outer candidate claims len 45 (58 B total) and swallows fa + fb + 1 stray byte */
  const uint8_t outer[11] = {0xA5, 0x5A, 45, 0, 1, 2, 3, 4, 5, 6, 0};
  memcpy(all, outer, 11); memcpy(all + 11, fa, 23); memcpy(all + 34, fb, 23);
  all[57] = 0; memcpy(all + 58, fc, 23);
  const uint8_t *exp[3] = {pa, pb, pc};
  int8_t rs[3] = {-1, -2, -3};
  int got = 0;
  ep_parser_init(&p);
  for (size_t i = 0; i < sizeof all; i++)
    if (ep_parser_feed(&p, all[i], &f)) {
      assert(got < 3);
      check_frame(&f, MAC, rs[got], exp[got], 10);
      got++;
    }
  assert(got == 3);
  assert(p.bad >= 1);
}

static void test_len_too_big(void) {
  const uint8_t hdr[4] = {0xA5, 0x5A, 0x00, 0x05};
  ep_parser_t p; ep_frame_t f;
  ep_parser_init(&p);
  assert(feed_all(&p, hdr, 4, &f) == 0);
  assert(p.bad == 1);
  assert(!ep_parser_mid_frame(&p));
}

static void test_mid_frame(void) {
  uint8_t pl[4] = {1, 2, 3, 4}, wire[20];
  ep_parser_t p; ep_frame_t f;
  size_t n = ep_encode(MAC, 0, pl, 4, wire, sizeof wire);
  ep_parser_init(&p);
  assert(!ep_parser_mid_frame(&p));
  feed_all(&p, wire, 5, &f);
  assert(ep_parser_mid_frame(&p));
  feed_all(&p, wire + 5, n - 5, &f);
  assert(!ep_parser_mid_frame(&p));
}

/* ---- reassembly ---- */

/* Builds packet i of a frame into pkt; returns packet length. */
static size_t mkpkt(uint8_t *pkt, const uint8_t *fr, size_t flen, uint8_t seq,
                    uint8_t i, uint8_t count) {
  size_t off = (size_t)i * EP_PKT_PAYLOAD, n = flen - off;
  if (n > EP_PKT_PAYLOAD) n = EP_PKT_PAYLOAD;
  pkt[0] = seq; pkt[1] = i; pkt[2] = count;
  memcpy(pkt + 3, fr + off, n);
  return n + 3;
}

static void test_single_packet(void) {
  uint8_t fr[100], pkt[250]; ep_reasm_t r; ep_frame_t f;
  fill(fr, 100, 2);
  ep_reasm_init(&r);
  size_t n = mkpkt(pkt, fr, 100, 1, 0, 1);
  assert(ep_reasm_push(&r, MAC, -33, pkt, n, 10, &f));
  check_frame(&f, MAC, -33, fr, 100);
}

static void test_max_frame(void) {
  uint8_t fr[1024], pkt[250]; ep_reasm_t r; ep_frame_t f;
  const uint8_t order[5] = {4, 0, 3, 1, 2};
  fill(fr, 1024, 8);
  ep_reasm_init(&r);
  for (int k = 0; k < 5; k++) {
    size_t n = mkpkt(pkt, fr, 1024, 3, order[k], 5);
    bool done = ep_reasm_push(&r, MAC, -50, pkt, n, 100 + k, &f);
    assert(done == (k == 4));
  }
  check_frame(&f, MAC, -50, fr, 1024);
}

static void test_duplicate_ignored(void) {
  uint8_t fr[300], pkt[250]; ep_reasm_t r; ep_frame_t f;
  fill(fr, 300, 4);
  ep_reasm_init(&r);
  size_t n = mkpkt(pkt, fr, 300, 1, 0, 2);
  assert(!ep_reasm_push(&r, MAC, -1, pkt, n, 1, &f));
  assert(!ep_reasm_push(&r, MAC, -1, pkt, n, 2, &f));
  n = mkpkt(pkt, fr, 300, 1, 1, 2);
  assert(ep_reasm_push(&r, MAC, -1, pkt, n, 3, &f));
  check_frame(&f, MAC, -1, fr, 300);
}

static void test_new_seq_discards(void) {
  uint8_t a[300], b[300], pkt[250]; ep_reasm_t r; ep_frame_t f;
  fill(a, 300, 1); fill(b, 300, 77);
  ep_reasm_init(&r);
  size_t n = mkpkt(pkt, a, 300, 7, 0, 2);
  assert(!ep_reasm_push(&r, MAC, -1, pkt, n, 1, &f));
  n = mkpkt(pkt, b, 300, 8, 0, 2);
  assert(!ep_reasm_push(&r, MAC, -1, pkt, n, 2, &f));
  n = mkpkt(pkt, b, 300, 8, 1, 2);
  assert(ep_reasm_push(&r, MAC, -1, pkt, n, 3, &f));
  check_frame(&f, MAC, -1, b, 300);
  assert(r.discarded == 1);
}

static void test_two_senders_interleaved(void) {
  const uint8_t A[6] = {0xA0, 0, 0, 0, 0, 1}, B[6] = {0xB0, 0, 0, 0, 0, 2};
  uint8_t fa[300], fb[300], pkt[250]; ep_reasm_t r; ep_frame_t f;
  fill(fa, 300, 11); fill(fb, 300, 99);
  ep_reasm_init(&r);
  size_t n = mkpkt(pkt, fa, 300, 1, 0, 2);
  assert(!ep_reasm_push(&r, A, -1, pkt, n, 1, &f));
  n = mkpkt(pkt, fb, 300, 1, 0, 2);
  assert(!ep_reasm_push(&r, B, -2, pkt, n, 2, &f));
  n = mkpkt(pkt, fb, 300, 1, 1, 2);
  assert(ep_reasm_push(&r, B, -2, pkt, n, 3, &f));
  check_frame(&f, B, -2, fb, 300);
  n = mkpkt(pkt, fa, 300, 1, 1, 2);
  assert(ep_reasm_push(&r, A, -1, pkt, n, 4, &f));
  check_frame(&f, A, -1, fa, 300);
}

static void test_eviction(void) {
  uint8_t fr[300], pkt[250]; ep_reasm_t r; ep_frame_t f;
  uint8_t m[5][6];
  fill(fr, 300, 6);
  ep_reasm_init(&r);
  for (int i = 0; i < 5; i++) { memset(m[i], 0x10 + i, 6); }
  for (int i = 0; i < 4; i++) {
    size_t n = mkpkt(pkt, fr, 300, 1, 0, 2);
    assert(!ep_reasm_push(&r, m[i], -1, pkt, n, i + 1, &f));
  }
  size_t n = mkpkt(pkt, fr, 300, 1, 0, 2);
  assert(!ep_reasm_push(&r, m[4], -1, pkt, n, 5, &f));
  n = mkpkt(pkt, fr, 300, 1, 1, 2);
  assert(ep_reasm_push(&r, m[1], -1, pkt, n, 6, &f));
  check_frame(&f, m[1], -1, fr, 300);
  assert(!ep_reasm_push(&r, m[0], -1, pkt, n, 7, &f));
}

static void test_rejects(void) {
  uint8_t pkt[260] = {0}; ep_reasm_t r; ep_frame_t f;
  ep_reasm_init(&r);
  assert(!ep_reasm_push(&r, MAC, 0, pkt, 2, 1, &f));            /* len < 3 */
  pkt[0] = 1; pkt[1] = 0; pkt[2] = 0;
  assert(!ep_reasm_push(&r, MAC, 0, pkt, 10, 1, &f));           /* count 0 */
  pkt[2] = 6;
  assert(!ep_reasm_push(&r, MAC, 0, pkt, 10, 1, &f));           /* count 6 */
  pkt[1] = 2; pkt[2] = 2;
  assert(!ep_reasm_push(&r, MAC, 0, pkt, 10, 1, &f));           /* idx >= count */
  pkt[1] = 4; pkt[2] = 5;                                       /* 988 + 247 > 1024 */
  assert(!ep_reasm_push(&r, MAC, 0, pkt, 250, 1, &f));
  assert(r.rejected == 5);
}

int main(void) {
  test_crc_check();
  test_roundtrip_small();
  test_roundtrip_max();
  test_garbage_prefix();
  test_bad_crc();
  test_false_sync_in_payload();
  test_nested_reject();
  test_swallowed_frames();
  test_len_too_big();
  test_mid_frame();
  test_single_packet();
  test_max_frame();
  test_duplicate_ignored();
  test_new_seq_discards();
  test_two_senders_interleaved();
  test_eviction();
  test_rejects();
  puts("proto ok");
  return 0;
}
