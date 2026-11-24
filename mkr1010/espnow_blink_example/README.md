# ESP-NOW blink example (MKR side)

Reference example: the ESP32-C3 SuperMini broadcasts a 1-byte ESP-NOW message at 1 Hz, the MKR WiFi 1010 blinks `LED_BUILTIN` once per message.
Sender lives in `../../esp32-c3-super-mini/espnow_blink_example/c3_sender/`.
Verified on hardware 2026-11-20: 10 blinks in 10 s.

## How the MKR receives ESP-NOW

The SAMD21 has no radio. The NINA-W102 module (an ESP32) does, but its stock WiFiNINA firmware cannot do ESP-NOW, so the NINA gets custom firmware:

C3 --ESP-NOW (ch 1, broadcast)--> NINA (`nina_receiver`) --UART raw payload @115200--> SAMD (`mkr_bridge`, `Serial1`) --> same bytes on USB serial

- `nina_receiver/`: runs on the NINA. Every ESP-NOW frame -> writes its payload bytes unchanged to UART0.
- `mkr_bridge/`: runs on the SAMD. Releases the NINA from reset, copies every `Serial1` byte to USB serial unchanged (the NINA boot log comes through too). No blinking anymore: this is the raw-binary receive test.

The NINA firmware lives in the NINA's own flash: it survives power cycles and normal SAMD sketch uploads. Any SAMD sketch must release the NINA from reset and read `Serial1` like `mkr_bridge` does. WiFiNINA does not work while this firmware is on the NINA.

## Gotchas found during bring-up

- `SerialNINA` is not defined for `mkrwifi1010` in SAMD core 1.8.14; the NINA link is `Serial1`.
- `NINA_RESETN` is active-high on this board: HIGH holds the NINA in reset, LOW runs it.
- NINA flash is 2 MB: build with `FlashSize=2M, PartitionScheme=minimal, FlashFreq=40, FlashMode=dio`. 80 MHz flash boot-loops ("partition 0 invalid magic number").
- The C3 SuperMini must use `WiFi.setTxPower(WIFI_POWER_8_5dBm)`; at full power its antenna barely radiates and nothing arrives. Broadcast "sent ok" is reported regardless, so it proves nothing.

## Flashing

Toolchain: arduino-cli, esp32:esp32 core 4.0.0, arduino:samd 1.8.14, WiFiNINA library 2.1.1 (only for the passthrough example).

1. Put the SAMD in passthrough mode so the NINA can be flashed over the MKR's USB port:
   `arduino-cli upload -p /dev/ttyACM0 --fqbn arduino:samd:mkrwifi1010 ~/Arduino/libraries/WiFiNINA/examples/Tools/SerialNINAPassthrough`
2. Flash the NINA (through the passthrough):
   `arduino-cli compile --upload -p /dev/ttyACM0 --fqbn esp32:esp32:esp32:FlashSize=2M,FlashFreq=40,FlashMode=dio,PartitionScheme=minimal,UploadSpeed=115200 nina_receiver`
3. Flash the SAMD bridge:
   `arduino-cli compile --upload -p /dev/ttyACM0 --fqbn arduino:samd:mkrwifi1010 mkr_bridge`
4. Flash the C3 sender:
   `arduino-cli compile --upload -p /dev/ttyACM1 --fqbn esp32:esp32:esp32c3:CDCOnBoot=cdc ../../esp32-c3-super-mini/espnow_blink_example/c3_sender`

Ports are as enumerated on the dev machine; check with `arduino-cli board list`.

## Restoring stock WiFiNINA firmware

A full backup of the original NINA flash is kept outside the repo, at `~/espnow-blink/nina_backup_2MB.bin` on the dev machine. With the passthrough sketch on the SAMD:
`~/.arduino15/packages/esp32/tools/esptool_py/5.3.1/esptool --chip esp32 --port /dev/ttyACM0 --baud 115200 write-flash -z 0x0 ~/espnow-blink/nina_backup_2MB.bin`
Untested. The Arduino IDE firmware updater is the fallback.
