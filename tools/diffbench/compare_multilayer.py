"""recompose_multilayer of the native machine against gpgx (spec 008, TP-205.2).

Runs a ROM for N frames on tessera_md_run and on gpgx (tessera_diffbench) and
compares the status, the size and the five RGB565 layers (plane A, plane B,
window, sprites, composite) recompose_multilayer returns after the last
frame. gpgx is subscribed to RECOMPOSITION; with --isolate also to
RENDER_CONTROLS, which draws every layer alone, and with --raster to
RASTER_TRACKING, which replays the raster journal.

usage: compare_multilayer.py <md_run> <diffbench> <gpgx_core> <rom> <frames>
                             <out_dir> [--isolate] [--raster]
exit:  0 identical, 1 different, 2 a tool failed
"""
import os
import struct
import subprocess
import sys

RECOMPOSITION, RENDER_CONTROLS, RASTER_TRACKING = 0x20, 0x04, 0x08
OPTIONS = ("--isolate", "--raster")
# gpgx statuses as Tessera returns them (see compare_recompose.py).
# RC_NOT_MODE5 and RC_JOURNAL_OVERFLOW are WRONG_STATE too.
STATUS = {-20: -5, -21: -5, -24: -5, -23: -1, -9: -28}
NAMES = ("A", "B", "W", "S", "C")


def load(path):
    with open(path, "rb") as f:
        data = f.read()
    status, width, height = struct.unpack_from("<iII", data)
    n = width * height * 2
    layers = [data[12 + n * k:12 + n * (k + 1)] for k in range(5)]
    return status, width, height, layers


def main(argv):
    args = [a for a in argv[1:] if a not in OPTIONS]
    isolate, raster = (option in argv for option in OPTIONS)
    if len(args) != 6:
        print(__doc__)
        return 2
    md_run, diffbench, core, rom, frames, out_dir = args
    os.makedirs(out_dir, exist_ok=True)
    mine = os.path.join(out_dir, "native.ml")
    theirs = os.path.join(out_dir, "gpgx.ml")
    if subprocess.run([md_run, "--rom", rom, "--frames", frames,
                       "--work-ram-out", os.path.join(out_dir, "unused.wram"),
                       "--multilayer-out", mine] +
                      (["--isolate-layers"] if isolate else []) +
                      (["--replay-journal"] if raster else []),
                      capture_output=True).returncode != 0:
        print("tool failed:", md_run)
        return 2
    mask = (RECOMPOSITION | (RENDER_CONTROLS if isolate else 0) |
            (RASTER_TRACKING if raster else 0))
    if subprocess.run([diffbench, "--libretro", core, "--rom", rom,
                       "--frames", frames, "--sub-mask", "%x" % mask,
                       "--multilayer-out", theirs],
                      capture_output=True).returncode not in (0, 1):
        print("tool failed:", diffbench)
        return 2
    a, b = load(mine), load(theirs)
    b = (STATUS.get(b[0], b[0]),) + b[1:]
    if a[:3] != b[:3]:
        print("status/size native %s gpgx %s" % (a[:3], b[:3]))
        return 1
    width = a[1]
    different = []
    for name, la, lb in zip(NAMES, a[3], b[3]):
        lines = sorted({i // 2 // width for i in range(0, len(la), 2)
                        if la[i:i + 2] != lb[i:i + 2]})
        if lines:
            different.append("%s: %d lines, first %s" %
                             (name, len(lines), lines[:6]))
    if different:
        print("layers differ; " + "; ".join(different))
        return 1
    print("identical status=%d %dx%d" % a[:3])
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
