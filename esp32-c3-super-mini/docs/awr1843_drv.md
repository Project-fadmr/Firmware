look at `TI.AWR1843AOPEVM.DRIVER.DEBUG.py`'s `Radar` class.
that's the driver that needs to be ported and adapted to GPIO18 and 19 (USB D- / D+)
the radar stays powered and configured. expose enable/disable as the handle BSP calls, instead of reconfiguring every time
