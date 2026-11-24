Files: `espnow_ll.c`, `espnow_ll.h`
Owner: @molly

ESP-NOW link layer.
Must configure the ESP-NOW radio for long range (LR mode) streaming, and expose a data interface and a power control interface.
Radar output with the current config is ~18-28 kbit/s (224-352 B frames at 10 fps, objects only), well within LR's ~0.5 Mbps.


<data interface>
send: hand over one radar frame, the link layer splits it into numbered ESP-NOW v1 packets (max 250 B each, ~2 per frame)
receive: if anything is ever sent back to the board
signal strength: RSSI of the two car-mode boards' broadcasts, compared by the supervisor in bikeMode

<power control interface>
radio on/off, so BSP and the supervisor can sleep between frames. While on, the radio wakes every 100 ms with a 5 ms window (matches the radar frame period)
TX power

<protocol>
ESP-NOW v1, so the Arduino MKR WiFi 1010 (NINA-W102) can receive without IDF >= 5.4 firmware

<packet format>
Each packet: 3-byte header [frame_seq (uint8, +1 per frame, wraps 255->0), packet_index (0-based), packet_count], then up to 247 B payload (packets <= 250 B).
LR only, so the receiver must also run in LR mode. TX power defaults to 8.5 dBm.
