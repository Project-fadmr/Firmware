# ESP-NOW wire format (shared by both boards)

This doc describes the ESP-NOW link between the ESP32-C3 SuperMini (radar node) and the Arduino MKR WiFi 1010 (NINA-W102 receiver), and the framing used to hand frames from the NINA on. Everything here comes from these files. Each fact names its source.

- `mkr1010/src/espnow_proto/espnow_proto.h` and `espnow_proto.c` (MKR side, short name "proto")
- `esp32-c3-super-mini/src/espnow_ll.h` and `espnow_ll.c` (C3 side, short name "ll")
- `esp32-c3-super-mini/docs/espnow_ll.md`
- `mkr1010/docs/espnow_drv.md`
- `mkr1010/src/nina_fw/nina_fw.ino` and `mkr1010/src/espnow_drv/espnow_drv.cpp` (NINA sender and SAMD receiver of the hand-off)
- `mkr1010/examples/espnow_blink_example/nina_receiver/nina_receiver.ino` (channel only)

Where a value is backed only by a doc, the line says so. Where the two sides disagree, the fact is listed under Open mismatches at the end.

## Radio (air interface)

- Mode: long range only. The C3 sets `WIFI_PROTOCOL_LR` (ll, espnow_ll.c, `link_layer_radio_on`). Both docs agree: `espnow_ll.md` says "LR only", and `espnow_drv.md` says "long range (LR) mode".
- Channel: 6 on the C3 (ll, espnow_ll.c, `ESPNOW_CHANNEL`) and on the MKR receivers (`mkr1010/examples/espnow_blink_example/nina_receiver/nina_receiver.ino` line 14; `mkr1010/src/nina_fw/nina_fw.ino` line 46). No secondary channel (`WIFI_SECOND_CHAN_NONE`, same function).
- Addressing: broadcast to FF:FF:FF:FF:FF:FF (ll, espnow_ll.c, `broadcast_mac`). The peer is added with encryption off (ll, same function).
- TX power: default 34 quarter-dBm, which is 8.5 dBm (ll, espnow_ll.c, `LL_TX_DEFAULT_QDBM`). `espnow_ll.md` also says 8.5 dBm. The setter accepts 8 to 84 quarter-dBm (ll, espnow_ll.c, `link_layer_set_tx_power`).
- Wake schedule while the radio is on: connectionless wake interval 100 ms, wake window 5 ms (ll, espnow_ll.c, `LL_WAKE_INTERVAL_MS` and `LL_WAKE_WINDOW_MS`). `espnow_ll.md` states the same values.
- Protocol version: ESP-NOW v1, so the NINA-W102 can receive it without newer IDF firmware (`espnow_ll.md`).

## Sizes

- Maximum radar frame: 1024 bytes on both sides (ll, `LL_MAX_FRAME` in espnow_ll.h; proto, `EP_MAX_FRAME` in espnow_proto.h).
- Maximum ESP-NOW packet: 250 bytes including the 3-byte packet header. Both docs say max 250 B (`espnow_ll.md`, `espnow_drv.md`). The code uses `ESP_NOW_MAX_DATA_LEN` (ll, espnow_ll.c line 27), whose value is not defined in this repo.
- Payload per packet: 247 bytes, which is 250 minus the 3-byte header (ll, espnow_ll.c, `LL_PAYLOAD_MAX`).
- Packets per frame: the sender uses ceil(length / 247) packets (ll, espnow_ll.c, `link_tx_task`). A 1024-byte frame needs 5 packets. The receiver accepts at most 5 (proto, espnow_proto.c, `ep_reasm_push`, check on `(count - 1) * EP_PKT_PAYLOAD`).

## ESP-NOW packet layout (fragmentation)

A frame is split into numbered packets. Each packet has a 3-byte header, followed by its payload.

- Byte 0: frame sequence number, uint8. The sender adds 1 for each frame and it wraps from 255 to 0 (ll, espnow_ll.c, `s_frame_seq`; `espnow_ll.md` says the same).
- Byte 1: packet index, 0-based (ll, espnow_ll.c, `pkt[1]`).
- Byte 2: packet count for this frame (ll, espnow_ll.c, `pkt[2]`).
- Bytes 3 onward: up to 247 bytes of frame data (ll, espnow_ll.c, `memcpy` into `pkt[LL_HDR_LEN]`).

The receiver's checks, in `ep_reasm_push` (proto, espnow_proto.c):

- A packet shorter than 3 bytes is rejected.
- A count of 0, an index at or past the count, or a frame longer than 1024 bytes is rejected.
- Every packet except the last must carry exactly 247 payload bytes. A short middle packet would leave a hole, so it is rejected.
- The receiver keeps up to 4 reassembly slots (proto, `EP_SLOTS`), keyed by source MAC. A slot is restarted when the sequence number or count changes, and the old partial frame is counted as discarded.
- A duplicate packet index in the same frame is ignored.
- When no slot is free, the oldest slot is reused.
- Slots have no timeout. A stale slot is replaced only by a new sequence number from the same MAC, or by age when a slot is needed.

The sender drops the whole frame if any packet fails to send or times out (ll, espnow_ll.c, `link_tx_task`). The receiver then sees an incomplete frame, which it handles as described above.

## NINA-to-SAMD framing (proto encoder)

`espnow_drv.md` says the NINA reassembles the packets and hands complete frames to the SAMD21 over UART. Those bytes are proto records: the NINA encodes each frame with `ep_encode` and writes it to its UART (`nina_fw.ino`, `loop`), and the SAMD21 decodes `Serial1` with `ep_parser_feed` (`espnow_drv.cpp`). Each record has these fields, with offsets from the first byte:

- Offset 0: sync byte 0xA5 (proto, `EP_SYNC0`).
- Offset 1: sync byte 0x5A (proto, `EP_SYNC1`).
- Offsets 2 and 3: payload length, uint16, little-endian. Maximum 1024 (proto, `ep_encode`).
- Offsets 4 to 9: source MAC, 6 bytes (proto, `ep_encode`).
- Offset 10: RSSI, int8 (proto, `ep_encode`).
- Offset 11 onward: payload, length bytes.
- The last 2 bytes: CRC-16, little-endian. The CRC is CRC-16/CCITT-FALSE with polynomial 0x1021, initial value 0xFFFF, no reflection and no final XOR, computed over offsets 2 through 10 plus the payload, which is length + 9 bytes (proto, `ep_crc16`).
- Total overhead: 13 bytes (proto, `EP_OVERHEAD`).

The parser (proto, `ep_parser_feed`) looks for the sync pair, rejects a length above 1024, and rejects a record whose CRC does not match. It counts each rejected record in `bad`.

## Open mismatches

None. Both sides agree on every fact above. If you change one side, update this doc and the other side together.
