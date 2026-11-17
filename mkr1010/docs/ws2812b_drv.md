Owner: @molly

Display driver for an 8x32 WS2812B matrix (256 LEDs).
Only pushes pixels. What gets drawn is decided by the supervisor.

<notes>
data pin: D5 (see bsp.md)
WS2812B data is 5 V, the SAMD21 is 3.3 V. use a level shifter (74AHCT125 or similar), like the rpi setup did
frame buffer: 256 x 3 B = 768 B
brightness capped at ~20%: full white at full brightness is ~15 A (256 x ~60 mA), at 20% it is ~3 A from the step-up converters

<open questions>
how are the 256 LEDs wired? (@molly confirms) current assumption: 32 columns of 8, each running bottom to top, driven as one 1D array of 256
what does the matrix show when no frame has arrived for a while? (@molly decides)
