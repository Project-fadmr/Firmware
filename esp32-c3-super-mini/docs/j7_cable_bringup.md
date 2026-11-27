Files: `tools/awr1843_drv_test/tools/test_single_uart.py`, `tools/awr1843_drv_test/tools/flash_mmwave.py`
Status: radar firmware patched and verified over USB on 2026-11-20. The J7 cable was not yet built.

Hand-off sheet for a future session: check that the home-made J7 cable carries the radar's single UART (commands + data) once it's assembled.


<state on 2026-11-20>
The AWR1843AOPEVM runs a patched TI mmWave SDK 03.06.02.00 AOP demo. Output frames go to the command UART (MSS UART-A) instead of the logger UART, so commands, replies and frames all share one UART.
Patch: `~/ti/mmwave_sdk_03_06_02_00-LTS/packages/ti/demo/xwr18xx/mmw/mss/mss_main.c`, 3 changes against `mss_main.c.orig`:
- frames are sent on `commandUartHandle` (line 2756)
- `commandBaudRate = 921600` (line 3381)
- `uartParams.writeDataMode = UART_DATA_BINARY` for UART 0 (line 3760), because text mode inserts `\r` before every 0x0A byte of a frame

Images in `~/ti/backup/` (checksums in `SHA256SUMS`):
- `xwr18xx_mmw_aop_demo_STOCK_03.06.02.00.bin`: TI original
- `xwr18xx_mmw_aop_demo_REBUILT_STOCK.bin`: stock source built locally
- `xwr18xx_mmw_aop_demo_SINGLEUART.bin`: what's flashed now (sha256 `dfd62840…7b2447`)

!! the real baud is 892857, not 921600. The radar divides 200 MHz by an integer (`uartsci.c:1735`), giving a −3.1 % error. The CP2105 tolerates it at 921600; the ESP32-C3 should be set to 892857 exactly.
!! replies end in `\n` only, never `\r\n` (binary write mode applies to command output too).
!! the Python debug driver (`tools/awr1843_drv_test/src/ti.awr1843aopevm.driver.debug.py`) still opens two ports and no longer works with this firmware. `tools/awr1843_drv_test/tools/test_single_uart.py` is the working reference.


<the cable>
EVM side: J7, a 2x5 1.27 mm right-angle male header (Sullins GRPB052MWCN-RC, TI BOM line 93).
Parts (Elextra): CONNFLY DS1017-01-10NA8 IDC socket (H56496) + 3M 10-way 0.635 mm AWG30 ribbon (H56657). The ribbon's red stripe marks pin 1.
J7 nets were traced in TI's layout file (`sprr449.zip` → `PROC106A1.PcbDoc`, pad-to-net data): radar UART-A → U18 mux (S1.1) → U15 mux (S1.2) → series resistor → J7. There's no level shifter on that path; the muxes and the radar IO run on 3.3 V rails.

| J7 pin   | Net (TI layout)                                 | Connect to        | Note                                                           |
|----------|-------------------------------------------------|-------------------|----------------------------------------------------------------|
| 1        | BT_UART_RX = radar TX (U2.U16 via U18/U15/R90)  | ESP / adapter RX  | Idle level should read ~3.3 V before connecting                |
| 2        | BT_UART_TX = radar RX (U2.V16 via U18/U15/R102) | ESP / adapter TX  |                                                                |
| 3        | nRESET (CC2640 U8.35)                           | GND               | Holds the Bluetooth chip in reset so it doesn't drive pins 1/2 |
| 5 or 7   | GND                                             | ESP / adapter GND |                                                                |
| 4/6/8/10 | JTAG (BT chip)                                  | nothing           |                                                                |
| 9        | VCC_BA_3V3                                      | nothing           | Don't power the ESP from it                                    |


<switches>
| Switch    | Normal run via J7 | Flashing (USB) | Normal run via USB (today's test) |
|-----------|-------------------|----------------|-----------------------------------|
| S1.1      | ON                | OFF            | OFF                               |
| S1.2      | OFF               | any            | any                               |
| S2.3      | OFF               | any            | any                               |
| S3 (SOP2) | OFF               | ON             | OFF                               |

Always press SW2 (reset) after changing switches.


<check procedure>
Ask the user first which host is on the far end of the cable: a 3.3 V USB-UART adapter (easiest; the test script runs unchanged) or the ESP32-C3. Don't assume.
1. Before powering: continuity-test the cable. Each J7 pin should reach exactly one wire, with no shorts between neighbours. Check the stripe is on pin 1.
2. Set the switches to "Normal run via J7", tie J7 pin 3 to GND, power the EVM, press SW2.
3. Measure J7 pin 1 to pin 5 with nothing else connected. It should idle at ~3.3 V. Anything near 1.8 V or 5 V: stop, don't connect the ESP.
4. Sanity check: `version` on the CP2105 port (`...-if00-port0`) must now get NO reply, because S1.1 ON moves the UART away from the CP2105. If it still answers, S1.1 isn't ON.
5. Connect pin 1 → host RX, pin 2 → host TX, GND → GND.
6. USB-UART adapter host: `cd tools/awr1843_drv_test && .venv/bin/python tools/test_single_uart.py /dev/ttyUSBx 921600` (use 892857 if the adapter supports it exactly).
   ESP32-C3 host: the firmware driver isn't written yet. Either port the same sequence (version → config → sensorStart → count frames → sensorStop → version) or use the C3 as a passthrough.
7. Compare with the baseline below. A pass = every row matches.

| Check                                   | Expected (baseline from 2026-11-20 over USB if00)                               |
|-----------------------------------------|---------------------------------------------------------------------------------|
| version                                 | Reply ends with 'Done' + 'mmwDemo:/\>', SDK 03.06.02.00, no '\r' in reply       |
| 27 config lines + sensorStop + flushCfg | Every command answers Done                                                      |
| sensorStart                             | Text 'Debug: Init Calibration Status = 0x1ffe', Done, prompt, then frames       |
| Frames in 2 s                           | ~38 complete frames (20 fps), 96 B each with 2 objects when the scene is static |
| Frame integrity                         | Frame numbers consecutive; each frame starts where the previous ends            |
| sensorStop                              | Done seen after the last frame                                                  |
| version after stop                      | OK                                                                              |


<troubleshooting>
| Symptom                                      | Likely cause                                                                                          | What to check                                                                                              |
|----------------------------------------------|-------------------------------------------------------------------------------------------------------|------------------------------------------------------------------------------------------------------------|
| No bytes at all, even after SW2 reset        | TX/RX swapped, S1.1 still OFF (UART goes to the CP2105), cable pin 1 on the wrong side, no common GND | Swap pins 1/2 at the far end; check switches; check the ribbon's pin-1 stripe vs J7 pin 1; continuity test |
| Garbage characters                           | Baud mismatch or a floating line                                                                      | Host at 921600 (USB adapter) or 892857 (ESP); check GND                                                    |
| Text works, frames corrupt or missing pieces | Bluetooth chip not held in reset and driving the lines; marginal cable                                | J7 pin 3 really at GND; re-press the IDC connector; shorten the cable                                      |
| Replies but every command fails              | Reply parsing expects \r\n                                                                            | Firmware sends \n only by design; parse on 'Done' + prompt                                                 |
| if00 on the PC answers but J7 doesn't        | S1.1 is OFF                                                                                           | Set S1.1 ON, S1.2 OFF, S2.3 OFF, press SW2                                                                 |
| Board dead / wrong firmware                  | Bad flash                                                                                             | Reflash a backup image (see Recovery)                                                                      |


<recovery: reflash>
Use `tools/awr1843_drv_test/tools/flash_mmwave.py`. It drives UniFlash's own mmWave gen1 flasher (`~/ti/uniflash_9.7.1/deskdb/content/TICloudAgent/linux/ccs_base/mmWave/gen1/mmWaveProgFlash.py`). The UniFlash `dslite.sh` CLI needs a GUI-exported `.ccxml`, so it isn't used.
1. Switches: S1.1 OFF, S3 ON, then press SW2.
2. Run `python3 tools/awr1843_drv_test/tools/flash_mmwave.py flash <if00 port> ~/ti/backup/<image>.bin` immediately. Run `check` OR `flash`, not both: the bootloader only answers the first connect after each reset, so another SW2 press is needed between runs.
3. Success ends with `SUCCESS: META_IMAGE1 written to SFLASH`. Then S3 OFF, press SW2.

Rebuilding: `bash -c 'cd ~/ti/mmwave_sdk_03_06_02_00-LTS/packages/scripts/unix && source ./setenv.sh && cd ../../ti/demo/xwr18xx/mmw && make mmwAOPDemo'`. This needs `mono`, and setenv.sh is already set to awr18xx / DOWNLOAD_FROM_CCS=no.
