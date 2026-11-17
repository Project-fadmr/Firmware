Files: `bsp.c`, `bsp.h`
Owners: @lilleole, @voxvoltera

Board Support Program for the ESP32-C3-Super-Mini.
Must control the board's functionality, power state, CPU speed, etc.
The supervisor calls into BSP. The other modules don't change board state themselves.


<responsibilities>
power states: active, light sleep, deep sleep, and the wake sources for each
CPU frequency: switch between low and high clock depending on load
pins: own the pin map (GPIO4 mode pin, GPIO18/19 radar USB) so it lives in one place. BSP reads/writes all pins (incl. GPIO4); only drivers may touch pins outside BSP
radar: stays powered and configured, BSP only calls the driver's enable/disable handle (reconfiguring every time wastes resources)
bikeMode wake source: ESP-NOW receive only. the board keeps listening for the car-mode boards' broadcasts, nothing else needs to wake it

<used by>
supervisor: picks power state and CPU speed per mode, see `supervisor.md`
