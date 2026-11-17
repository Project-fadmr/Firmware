Owner: @lilleole

Processing layer. Takes one radar frame and identifies subject type, distance and speed.

look at `../radar_data_approx_test/radar_data_aprox.py`. `parse`, `calc_frame` and `calc_delta_speed` need to be ported; the `Client` socket and `main` are PC scaffolding.

input: one complete AWR1843AOPEVM frame (TI mmWave SDK 3.x format, header + TLVs), see `../../../misc/docs/awr1843aopevm-uart-spec.md`
output: subject type, distance, speed, handed to the supervisor for the display

<what the script does now>
parse: reads the detected points (TLV 1: x, y, z, doppler) from each frame
calc_frame: averages the absolute x, y, z and doppler over all points in a frame
calc_delta_speed: change in those averages between two frames
there's no subject type yet

<speedometer>
own speed comes from a basic speedometer. too simple for its own driver, so it's read inside the processing layer

<subject type>
actual speed of the tracked object = speed measured by the radar, corrected by own speed
stationary object: < 10 km/h
bike: 10 - 40 km/h
car: > 40 km/h
the thresholds are constants at the top of the code. they get tuned during testing, which isn't done with a real bike for safety
