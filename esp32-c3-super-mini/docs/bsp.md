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
radar stream: while streaming, one BSP task reads frames from the driver and sends them with link_layer_send. Frames come every 100 ms: the task sleeps, wakes 15-25 ms early (15 ms guard, rounded down to whole ticks), holds the no-sleep lock only for the read, then re-aligns on each frame
GPIO21 (radar UART TX): pulled down only in light sleep, UART-controlled in active mode. awr1843_init sets it up after uart_set_pin
bikeMode wake source: ESP-NOW receive only. the board keeps listening for the car-mode boards' broadcasts, nothing else needs to wake it

<used by>
supervisor: picks power state and CPU speed per mode, see `supervisor.md`
