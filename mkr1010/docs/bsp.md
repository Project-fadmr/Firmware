Owners: @lilleole, @voxvoltera

Board Support Program for the Arduino MKR WiFi 1010 (SAMD21 + NINA-W102).
Same rules as the ESP32-C3 BSP: BSP owns the pin map and reads/writes all pins; only drivers may touch pins outside BSP.
The supervisor calls into BSP. The other modules don't change board state themselves.


<responsibilities>
power states and CPU speed of the SAMD21
NINA module: reset/enable pin, so the radio can be restarted or switched off
pins: own the pin map (NINA UART link, WS2812B data on D5, speedometer on D0)
speedometer: D0, interrupt-capable so pulses can be counted. sensor still undetermined
power: 3.7 V Li-ion batteries. the LED matrix runs off step-up converters (5 V)
