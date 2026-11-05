"""region_detect probe of the differential bench (spec 008, TP-108.4).

Runs the ROM on the native machine (tessera_md_run, region from the header)
and on gpgx and compares the bytes the probe stores in work RAM: the frame
counter (0..3), the I/O version register (4) and the lines per frame (6..7).
The rest of work RAM is the stack, whose return address depends on where in
the idle loop the interrupt landed, so it is not compared.

usage: compare_region_probe.py <md_run> <diffbench> <gpgx_core> <rom> <frames> <out_dir>
exit:  0 identical, 1 different, 2 a tool failed
"""
import os
import subprocess
import sys


def main(argv):
    md_run, diffbench, core, rom, frames, out_dir = argv[1:7]
    os.makedirs(out_dir, exist_ok=True)
    mine = os.path.join(out_dir, "native.wram")
    gpgx = os.path.join(out_dir, "gpgx.wram")
    runs = [
        [md_run, "--rom", rom, "--frames", frames, "--work-ram-out", mine],
        [diffbench, "--libretro", core, "--rom", rom, "--frames", frames,
         "--work-ram-out", gpgx],
    ]
    for command in runs:
        if subprocess.run(command, capture_output=True).returncode != 0:
            print("tool failed:", command[0])
            return 2
    with open(mine, "rb") as f:
        a = f.read()[:8]
    with open(gpgx, "rb") as f:
        g = bytearray(f.read())
    g[0::2], g[1::2] = g[1::2], g[0::2]  # gpgx hands work RAM word-swapped
    g = bytes(g[:8])
    print("version native=%02x gpgx=%02x lines native=%d gpgx=%d" % (
        a[4], g[4], int.from_bytes(a[6:8], "big"), int.from_bytes(g[6:8], "big")))
    differing = sum(1 for x, y in zip(a, g) if x != y)
    print("work_ram bytes=8 differing=%d" % differing)
    return 1 if differing else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
