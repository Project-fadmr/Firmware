"""
TI AWR1843AOPEVM DEBUG DRIVER TO RUN ON LINUX PC
Created by: voxvoltera | development@voxvoltera.com
version: 1
date: 17/11/2026
"""

import argparse
import asyncio
import logging
import os
import struct
import sys
from pathlib import Path

import serial

DATA_PORT = (
    "/dev/serial/by-id/usb-Silicon_Labs_CP2105_Dual_USB_to_UART_Bridge_Controller_"
    "019C69A2-if01-port0"
)
CLI_PORT = (
    "/dev/serial/by-id/usb-Silicon_Labs_CP2105_Dual_USB_to_UART_Bridge_Controller_"
    "019C69A2-if00-port0"
)
DATA_BAUD = 921600
CLI_BAUD = 115200
SOCK = "/tmp/stream.sock"

# from mmWave Demo Visualizer 3.6.0, xWR18xx_AOP, best_range: 50 m, 0.47 m res, ±11.83 m/s, 20 fps
RADAR_CFG = [
    "dfeDataOutputMode 1",
    "channelCfg 15 7 0",
    "adcCfg 2 1",
    "adcbufCfg -1 0 1 1 1",
    "profileCfg 0 77 8 7 18.64 0 0 30 1 133 12499 0 0 30",
    "chirpCfg 0 0 0 0 0 0 0 1",
    "chirpCfg 1 1 0 0 0 0 0 2",
    "chirpCfg 2 2 0 0 0 0 0 4",
    "frameCfg 0 2 16 0 50 1 0",
    "lowPower 0 0",
    "guiMonitor -1 1 0 0 0 0 0",
    "cfarCfg -1 0 2 8 4 3 0 15 1",
    "cfarCfg -1 1 0 4 2 3 1 15 1",
    "multiObjBeamForming -1 1 0.5",
    "clutterRemoval -1 0",
    "calibDcRangeSig -1 0 -5 8 256",
    "extendedMaxVelocity -1 0",
    "lvdsStreamCfg -1 0 0 0",
    "compRangeBiasAndRxChanPhase 0.0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0",
    "measureRangeBiasAndRxChanPhase 0 1.5 0.2",
    "CQRxSatMonitor 0 3 4 31 0",
    "CQSigImgMonitor 0 65 4",
    "analogMonitor 0 0",
    "aoaFovCfg -1 -90 90 -90 90",
    "cfarFovCfg -1 0 0 49.99",
    "cfarFovCfg -1 1 -12.18 12.18",
    "calibData 0 0 0",
]

MAGIC = b"\x02\x01\x04\x03\x06\x05\x08\x07"
HEADER_LEN = 40

MAX_PACKET_LEN = 64 * 1024

IDLE_TIMEOUT_S = 1.0
FILE_CHUNK = 4096

log = logging.getLogger("Texas Instruments AWR1843AOPEVM Driver")


class Radar:
    def __init__(self, cli_port, data_port):
        self.cli = None
        self.data = None

        self.cli_port = cli_port
        self.data_port = data_port

        try:
            self.cli = serial.Serial(self.cli_port, CLI_BAUD, timeout=IDLE_TIMEOUT_S)
            log.info("Opened CLI")
        except Exception as e:
            log.critical(f"Failed to open CLI interface with exception {e}")
            raise Exception("Error 02") from e

        try:
            self.data = serial.Serial(self.data_port, DATA_BAUD, timeout=IDLE_TIMEOUT_S)
            log.info("Opened DATA")
        except Exception as e:
            log.critical(f"Failed to open DATA interface with exception {e}")
            self.close()
            raise Exception("Error 03") from e

        self.enabled = False

    def _send(self, cmd):
        for _ in range(2):
            self.cli.reset_input_buffer()
            self.cli.write(f"{cmd}\n".encode())
            reply = self.cli.read_until(b"mmwDemo:/>")
            text = reply.decode(errors="replace")

            if "Done" in text:
                return
            if reply.endswith(b"mmwDemo:/>"):
                break
            log.warning(f"'{cmd}' timed out, retrying")

        log.critical(f"'{cmd}' failed, board replied: {text.strip()!r}")
        raise Exception("Error 04")

    def config(self):
        self._send("sensorStop")
        self.enabled = False
        self._send("flushCfg")
        for cmd in RADAR_CFG:
            self._send(cmd)
        log.info(f"Applied {len(RADAR_CFG)} configs")

    def enable(self):
        if self.enabled:
            log.info("Radar already enabled")
            return
        self.data.reset_input_buffer()
        self._send("sensorStart")
        self.enabled = True
        log.info("Radar enabled")

    def read(self):
        synced = self.data.read_until(MAGIC)
        if not synced.endswith(MAGIC):
            log.debug("No frame within timeout")
            return None
        if len(synced) > len(MAGIC):
            log.debug("Skipped %d bytes before magic word", len(synced) - len(MAGIC))

        header = self.data.read(HEADER_LEN - len(MAGIC))
        if len(header) < HEADER_LEN - len(MAGIC):
            log.warning("Timed out mid-header")
            return None

        length = struct.unpack_from("<I", header, 4)[0]
        if not HEADER_LEN <= length <= MAX_PACKET_LEN:
            log.warning("Invalid frame length %d, resyncing", length)
            return None

        body = self.data.read(length - HEADER_LEN)
        if len(body) < length - HEADER_LEN:
            log.warning(
                "Timed out mid-frame (%d of %d bytes)", len(body), length - HEADER_LEN
            )
            return None

        return MAGIC + header + body

    def disable(self):
        if not self.enabled:
            log.info("Radar already disabled")
            return
        self._send("sensorStop")
        self.enabled = False
        log.info("Radar disabled")

    def reset(self):
        try:
            self._send("sensorStop")
            self.enabled = False
            self._send("flushCfg")
            log.info("reset successful")
        except Exception as e:
            log.critical(f"RESET FAILED! BOARD IS IN UNKNOWN STATE\n{e}")
            raise Exception("Error 05") from e

    def close(self):
        if self.cli is not None:
            self.cli.close()
            self.cli = None
            log.info("closed CLI")
        else:
            log.info("CLI already closed")
        if self.data is not None:
            self.data.close()
            self.data = None
            log.info("closed DATA")
        else:
            log.info("DATA already closed")


class Socket:
    def __init__(self, path=SOCK, max_backlog=4 * 1024 * 1024):
        self.path = path
        self.max_backlog = max_backlog
        self.clients = set()
        self.server = None

    async def start(self):
        if os.path.exists(self.path):
            log.debug("Removing stale socket %s", self.path)
            os.unlink(self.path)
        try:
            self.server = await asyncio.start_unix_server(self.handle, path=self.path)
            log.info("Listening on UNIX socket %s", self.path)
        except Exception as e:
            log.critical(f"Failed to create socket at {self.path} with exception {e}")
            raise Exception("Error 01") from e

    async def handle(self, reader: asyncio.StreamReader, writer: asyncio.StreamWriter):
        self.clients.add(writer)
        log.info("Client connected (%d total)", len(self.clients))
        try:
            while await reader.read(FILE_CHUNK):
                pass
        except ConnectionError:
            log.exception("Connection error on UNIX socket")
        finally:
            self.clients.discard(writer)
            writer.close()
            log.info("Client disconnected (%d left)", len(self.clients))

    def send(self, data):
        for w in list(self.clients):
            if w.is_closing() or w.transport.get_write_buffer_size() > self.max_backlog:
                if not w.is_closing():
                    log.warning(
                        "Dropping slow client: backlog over %d bytes", self.max_backlog
                    )
                self.clients.discard(w)
                w.close()
            else:
                w.write(data)

    def close(self):
        if self.server:
            self.server.close()
        for w in self.clients:
            w.close()
        self.clients.clear()
        if os.path.exists(self.path):
            os.unlink(self.path)
        log.info("UNIX socket %s closed", self.path)


def main():
    # control flow of the program
    args = parse_args()
    logging.basicConfig(
        level=args.log_level, format="%(asctime)s %(levelname)s %(name)s: %(message)s"
    )
    try:
        asyncio.run(run(args))
    except KeyboardInterrupt:
        log.info("Stopped by user")
    except Exception as e:
        sys.exit(str(e))


async def run(args):
    # feeds the socket from the dummy file or the radar
    socket = Socket(args.socket)
    radar = None
    try:
        await socket.start()

        if args.file is not None:
            log.info("Waiting for a client before feeding %s", args.file)
            while not socket.clients:
                await asyncio.sleep(0.1)
            socket.send(await asyncio.to_thread(Path(args.file).read_bytes))
            for w in list(socket.clients):
                await w.drain()
            log.info(f"Fed {args.file}")
            return

        radar = Radar(args.cli_port, args.data_port)
        radar.config()
        radar.enable()
        while True:
            frame = await asyncio.to_thread(radar.read)
            if frame is not None:
                socket.send(frame)
    finally:
        if radar is not None:
            try:
                radar.disable()
            except Exception as e:
                log.error(f"Failed to disable radar on shutdown: {e}")
            radar.close()
        socket.close()


def parse_args():
    # help display with argparser
    ap = argparse.ArgumentParser(description="Manage the AWR1843OPEVM radar")
    ap.add_argument(
        "--data_port", default=DATA_PORT, help="data port (default: CP2105 if01 by-id)"
    )
    ap.add_argument(
        "--cli_port", default=CLI_PORT, help="cli port (default: CP2105 if00 by-id)"
    )
    ap.add_argument("--file", metavar="X.bin", help="feed a dummy file to the pipeline")
    ap.add_argument(
        "--socket",
        default=SOCK,
        help="specify socket location (default: /tmp/stream.sock)",
    )
    ap.add_argument(
        "--log-level",
        default="INFO",
        type=str.upper,
        choices=["DEBUG", "INFO", "WARNING", "ERROR", "CRITICAL"],
        help="logging verbosity (default: INFO)",
    )
    return ap.parse_args()


if __name__ == "__main__":
    main()
