# Smoke test for the single-UART radar firmware: commands + frames on one port.
# Usage: test_single_uart.py [PORT] [BAUD]   (default: EVM CP2105 if00 at 921600)
import importlib.util, os, struct, sys, time
import serial

DRV = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src", "ti.awr1843aopevm.driver.debug.py")
spec = importlib.util.spec_from_file_location("dbg", DRV)
dbg = importlib.util.module_from_spec(spec)
spec.loader.exec_module(dbg)  # only for RADAR_CFG / MAGIC / PROMPT

PORT = sys.argv[1] if len(sys.argv) > 1 else "/dev/serial/by-id/usb-Silicon_Labs_CP2105_Dual_USB_to_UART_Bridge_Controller_019C696E-if00-port0"
BAUD = int(sys.argv[2]) if len(sys.argv) > 2 else 921600
s = serial.Serial(PORT, BAUD, timeout=1)


def cmd(c):
    s.write(f"{c}\n".encode())
    r = s.read_until(dbg.PROMPT)
    ok = b"Done" in r and r.endswith(dbg.PROMPT)
    return ok, r


s.reset_input_buffer()
ok, r = cmd("version")
print("version:", "OK" if ok else "FAIL", "| has \\r:", b"\r" in r)
print(r.decode(errors="replace").strip()[:400])
if not ok:
    sys.exit(1)

fails = [c for c in ["sensorStop", "flushCfg", *dbg.RADAR_CFG] if not cmd(c)[0]]
print("config:", "all Done" if not fails else f"FAILED: {fails}")

s.write(b"sensorStart\n")
time.sleep(2.0)
buf = s.read(s.in_waiting or 1)
print("bytes in 2 s after sensorStart:", len(buf))

frames, i = [], buf.find(dbg.MAGIC)
pre = buf[:i] if i >= 0 else buf
while 0 <= i and i + dbg.HEADER_LEN <= len(buf):
    total, fnum, nobj = (struct.unpack_from("<I", buf, i + o)[0] for o in (12, 20, 28))
    if i + total > len(buf):
        break
    nxt = buf.find(dbg.MAGIC, i + total)
    frames.append((fnum, total, nobj, nxt in (-1, i + total)))
    i = nxt
print("text before first frame:", pre.decode(errors="replace").strip()[:120])
print("complete frames:", len(frames))
for f in frames[:5]:
    print("  frame#%d len=%d objs=%d next-frame-contiguous=%s" % f)
nums = [f[0] for f in frames]
print("frame numbers consecutive:", nums == list(range(nums[0], nums[0] + len(nums))) if nums else None)

s.write(b"sensorStop\n")
time.sleep(0.5)
tail = s.read(s.in_waiting or 1)
print("sensorStop: 'Done' seen in tail:", b"Done" in tail)
s.reset_input_buffer()
print("version after stop:", "OK" if cmd("version")[0] else "FAIL")
s.close()
