Owners: @lilleole, @voxvoltera

ESP-NOW driver. The stock NINA firmware and the WiFiNINA library have no ESP-NOW, so this is two parts:
1. custom firmware on the NINA-W102 (ESP32) that receives ESP-NOW
2. a driver on the SAMD21 that talks to that firmware and hands complete frames to the supervisor

Receive only. Must match the sender side, see `../../esp32-c3-super-mini/docs/espnow_ll.md`:
ESP-NOW v1, long range (LR) mode, radar frames split into numbered packets of max 250 B. the NINA firmware puts them back together, so the SAMD21 only receives whole frames over UART.
also exposes RSSI if the supervisor ever needs it.


<NINA firmware>
base: https://github.com/arduino/nina-fw (ESP-IDF v4.4.8, has ESP-NOW v1 and LR)
build: `RELEASE=1 make`, optionally `python combine.py` for one `NINA_W102.bin` flashed at 0x0
flash: upload the WiFiNINA `Tools -> SerialNINAPassthrough` example to the MKR, then flash with `esptool` at 115200 (default reset) or `arduino-fwuploader`
undo: flash the stock firmware again with the IDE 2 Firmware Updater
once flashed, WiFiNINA no longer works. NINA <-> SAMD21 is a UART link

Protocol details, including field offsets: `../../docs/espnow_protocol.md`.
