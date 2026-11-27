<p align="center">
  <img src="https://github.com/Project-Fylgja/.github/blob/main/fadmr-wide.png" alt="Large Centered Image" width="100%">
</p>

## Boards

The system uses two boards.

The ESP32-C3 SuperMini is the radar node. It reads the TI AWR1843AOPEVM over UART and sends the radar frames over ESP-NOW.

The Arduino MKR WiFi 1010 is the display and supervisor. Its NINA-W102 module receives the ESP-NOW frames and forwards them to the SAMD21. The SAMD21 processes each frame and draws the result on the 8x32 WS2812B matrix.

Data path: AWR1843AOPEVM -> UART -> ESP32-C3 -> ESP-NOW -> NINA-W102 -> SAMD21 -> WS2812B matrix. The NINA and the SAMD21 are linked by UART, see [docs/espnow_protocol.md](docs/espnow_protocol.md).

## Build and docs

Build instructions live in each board's docs folder:

- ESP32-C3 SuperMini: [esp32-c3-super-mini/docs/](esp32-c3-super-mini/docs/)
- Arduino MKR WiFi 1010: [mkr1010/docs/](mkr1010/docs/)
- Shared ESP-NOW wire format: [docs/espnow_protocol.md](docs/espnow_protocol.md)

## Folder layout

Each board folder has the same shape:

- `src/`: firmware sources. On the MKR, each module has its own folder under `src/` (bsp, display, espnow_drv, espnow_proto, main, nina_fw, radar). On the C3, the sources sit directly in `src/`.
- `docs/`: notes for each module.
- `examples/`: standalone sketches and programs that exercise the board.
- `tools/`: scripts that run on a PC.
- `test/`: host unit tests, MKR only. Run them with `make test` in `mkr1010/test`.

On the MKR, each module that has examples keeps them in its own `src/<module>/examples/` folder, for example `mkr1010/src/bsp/examples/`.
