# ESP-NOW blink example (C3 side)

`c3_sender/` broadcasts a 1-byte ESP-NOW message (`'B'`, channel 6) once per second; the MKR WiFi 1010 blinks its LED for each one.
Receiver side, wiring of the data path, gotchas and flashing steps: `../../../mkr1010/examples/espnow_blink_example/README.md`.

Note: TX power is set to 8.5 dBm on purpose. The SuperMini's antenna barely radiates at full power, and the MKR received nothing until this was lowered.

Flash: `arduino-cli compile --upload -p /dev/ttyACM1 --fqbn esp32:esp32:esp32c3:CDCOnBoot=cdc c3_sender`
