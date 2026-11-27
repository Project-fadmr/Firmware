# MKR WiFi 1010 BSP + ESP-NOW receive path: design

Date: 2026-11-27
Status: awaiting review

## Goal

Give the MKR WiFi 1010 a foundation to build on. That is a Board Support Program (BSP) that owns the board, a cooperative scheduler, and a receive path that delivers complete, verified radar frames from any ESP32-C3 in range. There is no supervisor layer. The BSP and the drivers take over its jobs, and the main sketch only initialises modules and runs the scheduler.

## Decisions (agreed 2026-11-27)

- Scope: BSP, NINA firmware, the SAMD-side ESP-NOW driver, a thin main sketch, and the display driver change needed for the handshake.
- No supervisor. `supervisor.md` is retired. The old rule "the supervisor calls into the BSP, modules don't change board state themselves" is replaced by: the BSP owns the pins and the power policy, and drivers only touch the pins the BSP hands them.
- NINA to SAMD link: UART (`Serial1`), not SPI.
- NINA firmware: an Arduino-ESP32 sketch (esp32 core 4.0.0, IDF 5.x) built from the proven `nina_receiver`. It is not a nina-fw fork.
- The NINA reassembles frames. The SAMD only ever sees whole frames.
- Several C3s may be in range at once. Frames are tracked and reported per sender.
- ESP-NOW channel: 6 (the C3 link layer already uses it, so the C3 is not changed). LR mode, ESP-NOW v1.
- WS2812B data pin: 8.
- No speedometer. D0 is not part of the pin map.
- No CPU clock switching. USB, UART baud and WS2812B timing all need 48 MHz.
- Power: idle standby, owned by the BSP. After 5 s without a frame, the BSP blanks the matrix and puts the SAMD into idle sleep (CPU stopped, clocks, UART and USB running). The NINA keeps listening. The next frame wakes the SAMD and ends standby.
- UART framing: sync bytes + header + CRC16, at 460800 baud.
- Display/UART conflict: solved with a busy handshake wire on `NINA_ACK`, not a DMA LED library.
- The BSP includes a cooperative scheduler, like the C3 BSP.

## Components

### 1. NINA firmware: `mkr1010/src/nina_fw/`

An Arduino-ESP32 sketch that replaces `nina_receiver`.

- Wi-Fi in STA mode with no connection, LR protocol only, channel 6, ESP-NOW receive callback registered.
- Reassembly table with 4 slots (constant), one per sender MAC.
- Completed frames go into an 8-frame queue. The sketch's `loop()` takes frames off the queue and writes them to UART0 at 460800. The ESP-NOW callback never writes to the UART.
- Before starting each frame, the NINA reads the busy line (GPIO33, input). If busy is high, the frame waits in the queue. A frame that has started is always finished, written in one call.
- One-way only. It never transmits over ESP-NOW and never receives commands from the SAMD.

### 2. SAMD ESP-NOW driver: `mkr1010/src/espnow_drv/`

- Reads `Serial1`, searches for sync bytes, checks length (at most 1024) and CRC16.
- **Poll interface:** called from a scheduler job. It returns either "no frame" or one complete frame: sender MAC, RSSI, payload, length.
- **Hold / release:** hold raises busy through the BSP, waits a ~1 ms guard, then reads to the end of any frame already in flight, keeping it for the next poll. Release drops busy.
- **Activity:** every verified frame is reported to the BSP so it can reset the idle timer.
- **Counters:** frames received, frames dropped for bad CRC or length, frames dropped for buffer overflow.
- Touches no pins directly. It asks the BSP to start the NINA.

### 3. BSP: `mkr1010/src/bsp/`

- **Pin map:** `NINA_RESETN` (active-high: HIGH holds reset), `NINA_GPIO0` (HIGH at reset so the NINA boots its app), `Serial1` to the NINA, the busy line `NINA_ACK` (Arduino pin 35, SAMD output), and WS2812B data on pin 8. The display driver gets its pin from the BSP instead of a hard-coded 8.
- **NINA control:** on, off (held in reset), and restart.
- **Busy line:** set and clear. Only the ESP-NOW driver calls these.
- **Scheduler:** the same model and API shape as the C3 BSP (`bsp.h`): init, add a job with a period in ms (returns a job id, or -1 if the table is full or the arguments are bad, with the first run one period after add), remove a job. Up to 8 jobs (constant), run cooperatively. Jobs must be short and never block. Jobs may add or remove jobs. The SAMD has no RTOS, so instead of a start call that creates a task, the main sketch calls a run function from `loop()`. It runs every job that is due, based on `millis()`, with 1 ms resolution. Due jobs may be delayed while a redraw holds the UART (up to ~22 ms).
- **Idle standby (policy):** the BSP keeps the time of the last frame (reported by the ESP-NOW driver). A BSP scheduler job checks it. After 5 s (constant) without a frame, the BSP blanks the matrix once through the display driver and enters standby. While in standby, the scheduler's run function puts the SAMD into idle sleep at the end of each pass, and any interrupt (UART byte, 1 ms SysTick) wakes it. The next reported frame ends standby. Like the C3 BSP calling the radar driver's enable/disable, the BSP calls the display driver's clear/show here.

### 4. Display driver change: `mkr1010/src/display/`

- `sm_show()` wraps the LED write in the ESP-NOW driver's hold and release. Every redraw is protected, so callers don't have to remember. Each redraw may wait up to ~22 ms for a frame already in flight.
- `sm_begin()` takes its data pin from the BSP's pin map.
- Drawing logic is unchanged.

### 5. Main sketch: `mkr1010/src/main/`

Thin wiring, like `main.c` on the C3. Setup initialises the BSP, starts the NINA, initialises the ESP-NOW driver and the display, and adds the frame job to the scheduler. `loop()` calls the scheduler's run function and nothing else.

The frame job polls the ESP-NOW driver and hands each frame to processing (your buddy's code), which decides what to draw and calls the display driver. Until processing exists, the frame job prints frame info (sender, RSSI, length) to USB serial instead.

### 6. C3 changes

None. The C3 link layer is already on channel 6.

### 7. Docs

- `mkr1010/docs/bsp.md`: UART link, pin 8, busy line, scheduler, idle standby policy, the new ownership rule. Remove the speedometer and CPU speed items.
- `mkr1010/docs/espnow_drv.md`: Arduino-ESP32 sketch instead of nina-fw, UART instead of SPI, framing, hold/release, multiple senders.
- `mkr1010/docs/ws2812b_drv.md`: pin 8 from the BSP, `sm_show()` holds the UART.
- `mkr1010/docs/supervisor.md`: deleted.
- `mkr1010/espnow_blink_example/README.md`: points to `src/nina_fw` instead of `nina_receiver`.
- `mkr1010/docs/processing.md` is not edited. Its owner gets a note that the speedometer was dropped, that there is no supervisor (processing is called from the frame job and drives the display itself), and that redraws may take up to ~22 ms longer.

## Data flow

C3 (ESP-NOW, ch 6, LR, broadcast) -> NINA (per-sender reassembly -> queue -> busy check -> UART 460800) -> SAMD ESP-NOW driver (sync/length/CRC check, reports activity to the BSP) -> frame job (scheduler) -> processing -> display driver (`sm_show()` wrapped in hold/release).

### Packet format (from the C3, unchanged)

3-byte header: frame_seq (uint8, +1 per frame, wraps), packet_index (0-based), packet_count. Then up to 247 B of payload. Packet i goes at offset i x 247 in the frame.

### UART frame format (NINA to SAMD)

In order: sync bytes 0xA5 0x5A, payload length (uint16, little-endian), sender MAC (6 B), RSSI (int8), payload, then CRC16 (little-endian) over everything from the length field to the end of the payload. CRC16 is CRC-16/CCITT-FALSE: polynomial 0x1021, initial value 0xFFFF, no reflection, no final XOR.

## Error handling

NINA reassembly:
- Reject a packet shorter than 3 B, a packet_count of 0, a count that would make the frame larger than 1024 B, or packet_index >= packet_count.
- A new frame_seq from the same sender discards that sender's unfinished frame.
- Duplicate packets are ignored.
- When all 4 slots are full, a new sender takes the slot that has been quiet the longest. There is no timeout timer.
- The frame's RSSI is the RSSI of its last packet.
- When the queue is full, the oldest frame is dropped and counted.

SAMD driver:
- A bad length or CRC drops the frame, counts it, and restarts the sync search one byte after the false sync. Garbage such as the NINA boot log is skipped this way.
- No reassembly and no timeouts.

Known remaining edge case: if the NINA's task is preempted between reading busy and sending the first byte for longer than the guard time, that frame overlaps a redraw. The CRC catches it and the frame is dropped and counted. The next frame arrives 100 ms later.

## Testing

PC unit tests (plain C, no hardware):
- NINA reassembly: out-of-order packets, duplicates, a new frame_seq mid-frame, slot eviction, rejection of bad headers and oversized frames.
- SAMD parser: garbage prefix, bad CRC, false sync inside the payload, a frame split across reads.
- Scheduler: due-time ordering, add/remove from inside a job, a full table, `millis()` wraparound.

Hardware, in order:
1. Busy line: toggling `NINA_ACK` on the SAMD is seen on NINA GPIO33.
2. One C3 on ch 6 -> NINA -> SAMD prints verified frames, with drop counters at 0.
3. Two C3s at once: both senders reassembled correctly.
4. A test job redrawing at 10 fps while streaming: drop counter stays at 0.
5. The same test with the hold/release bypassed: the drop counter rises, which shows the test detects the problem.
6. Idle standby: stop the C3s, after 5 s the matrix blanks and the SAMD sleeps. Restart a C3: the first frame wakes it and arrives intact.

## Open items

1. **Busy line pin: decided.** SAMD `NINA_ACK` (Arduino pin 35 in the `mkrwifi1010` variant) -> NINA GPIO33. This is the line stock nina-fw uses as its SPI ready/ACK (`SPISClass SPIS(VSPI_HOST, 1, 12, 23, 18, 5, 33)`, readyPin = 33), so the board connects it. GPIO33 is not an ESP32 boot-strapping pin. The SPI MOSI line (GPIO12, the MTDI strap that selects flash voltage) is deliberately avoided. Direction is reversed from stock: the SAMD drives the line and the NINA reads it. Hardware test step 1 confirms it.
2. **Idle timeout: decided.** 5 s, a BSP constant.
3. **Redraw delay: contained.** It stays inside `sm_show()` (up to ~22 ms). The only consumer to inform is processing, via the note above.
