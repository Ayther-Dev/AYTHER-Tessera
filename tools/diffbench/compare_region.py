"""Console region of the differential bench (spec 008, TP-108.4).

Runs every ROM on the native machine (tessera_md_run --print-region, region
read from the header) and on gpgx (SYSTEM region, id 18) and compares the PAL
flag and the lines per frame. gpgx decides its own region from the ROM header,
so this checks the header rule on real cartridges.

usage: compare_region.py <md_run> <diffbench> <gpgx_core> <out_dir> <rom>...
exit:  0 identical, 1 different, 2 a tool failed
"""
import os
import re
import struct
import subprocess
import sys


def main(argv):
    md_run, diffbench, core, out_dir = argv[1:5]
    roms = argv[5:]
    os.makedirs(out_dir, exist_ok=True)
    differing = 0
    for rom in roms:
        blob = os.path.join(out_dir, "system.bin")
        if os.path.exists(blob):
            os.remove(blob)
        native = subprocess.run(
            [md_run, "--rom", rom, "--frames", "1", "--print-region",
             "--work-ram-out", os.path.join(out_dir, "native.wram")],
            capture_output=True, text=True)
        oracle = subprocess.run(
            [diffbench, "--libretro", core, "--rom", rom, "--frames", "30",
             "--dump-regions", "--region-out", "18:" + blob],
            capture_output=True, text=True)
        match = re.search(r"pal=(\d)", native.stdout)
        if native.returncode != 0 or not match or not os.path.exists(blob):
            print("tool failed:", rom)
            return 2
        with open(blob, "rb") as f:
            data = f.read()
        # SYSTEM: u32 size, u32 layout, u8 system_hw, u8 region_pal, ...
        # u16 lines_per_frame at offset 14.
        gpgx_pal = data[9]
        lines = struct.unpack_from("<H", data, 14)[0]
        same = int(match.group(1)) == gpgx_pal and lines == (313 if gpgx_pal else 262)
        print("%s native_pal=%s gpgx_pal=%d lines=%d %s" % (
            os.path.basename(rom), match.group(1), gpgx_pal, lines,
            "same" if same else "DIFFERENT"))
        differing += 0 if same else 1
    print("roms=%d differing=%d" % (len(roms), differing))
    return 1 if differing else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
