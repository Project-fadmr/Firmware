# Drives UniFlash's own mmWave gen1 flasher (mmWaveProgFlash.BootLdr) the same way
# UniFlash's FlashPython.load_image does. Usage:
#   flash_mmwave.py check PORT          -> connect + read bootloader version, no writes
#   flash_mmwave.py flash PORT IMAGE    -> format SFLASH + write IMAGE as META_IMAGE1
import sys
GEN1 = "/home/voxy/ti/uniflash_9.7.1/deskdb/content/TICloudAgent/linux/ccs_base/mmWave/gen1"
sys.path[:0] = [GEN1, GEN1 + "/../common/pyserial-3.5/"]
import mmWaveProgFlash as m

PART = "AWR1843AOP"


class Cb:  # stands in for UniFlash's FlashPythonInstance callbacks
    def push_message(self, msg, level):
        if level != m.FLASHPYTHON_DEBUG_LEVEL:
            print(msg, flush=True)

    def update_progress(self, msg, pct):
        pass

    def check_is_cancel_set(self):
        return False


def main():
    mode, port = sys.argv[1], sys.argv[2]
    ldr = m.BootLdr(Cb(), port)
    if not ldr.connect(1, port):
        sys.exit("connect failed: is the board in flashing mode (S3 ON + SW2 reset, S1.1 OFF)?")
    try:
        ldr.setPartNum(PART)
        if not ldr.determinePGVersion():
            sys.exit("could not read bootloader version")
        print("bootloader answered; PG3 or later:", ldr.isDevicePG3OrLater(), flush=True)
        if mode != "flash":
            return
        img = m.FilesObject(sys.argv[3], 1)  # order 1 = META_IMAGE1
        files = ldr.copyImagesList([img])
        ldr.addAutomaticDownload(files, "")
        assert all(ldr.checkFileHeader(f.path, f) for f in files), "image header rejected"
        ldr.calcProgressValues(files, sum(f.fileSize for f in files), True)
        ldr.erase_storage()
        for f in files:
            if not ldr.download_file(f.path, f.file_id, 0, 0, "SFLASH", ldr.getImageProgCntList(f)):
                sys.exit("download FAILED for " + f.path)
            print("SUCCESS:", f.file_id, "written to SFLASH", flush=True)
    finally:
        ldr.disconnect()


main()
