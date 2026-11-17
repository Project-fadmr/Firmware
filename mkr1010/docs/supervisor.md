Owner: @voxvoltera

Arduino MKR WiFi 1010 supervisor. Replaces the rpi in the project.

<loop>
1. espnow_drv hands over a complete radar frame
2. processing turns it into subject type, distance and speed
3. ws2812b_drv draws the result on the 8x32 matrix

<applies always>
BSP should always be invoked whenever appropriate (power state, CPU speed)
