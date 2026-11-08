"""recompose_frame of the native machine against gpgx (spec 008, TP-205.1).

Runs a ROM for N frames on tessera_md_run and on gpgx (tessera_diffbench,
subscribed to RECOMPOSITION) and compares the status, the size and the RGB565
pixels recompose_frame returns after the last frame.

usage: compare_recompose.py <md_run> <diffbench> <gpgx_core> <rom> <frames>
                            <out_dir>
exit:  0 identical, 1 different, 2 a tool failed
"""
import os
import struct
import subprocess
import sys

RECOMPOSITION = "20"
# gpgx statuses as Tessera returns them: RC_INTERLACE2 -> WRONG_STATE,
# RC_INVALID_PARAMS -> INVALID_ARGUMENT, NOT_SUBSCRIBED -> NOT_SUBSCRIBED.
STATUS = {-21: -5, -23: -1, -9: -28}


def load(path):
    with open(path, "rb") as f:
        data = f.read()
    status, width, height = struct.unpack_from("<iII", data)
    return status, width, height, data[12:]


def main(argv):
    if len(argv) != 7:
        print(__doc__)
        return 2
    md_run, diffbench, core, rom, frames, out_dir = argv[1:]
    os.makedirs(out_dir, exist_ok=True)
    mine = os.path.join(out_dir, "native.rc")
    theirs = os.path.join(out_dir, "gpgx.rc")
    if subprocess.run([md_run, "--rom", rom, "--frames", frames,
                       "--work-ram-out", os.path.join(out_dir, "unused.wram"),
                       "--recompose-out", mine],
                      capture_output=True).returncode != 0:
        print("tool failed:", md_run)
        return 2
    if subprocess.run([diffbench, "--libretro", core, "--rom", rom,
                       "--frames", frames, "--sub-mask", RECOMPOSITION,
                       "--recompose-out", theirs],
                      capture_output=True).returncode not in (0, 1):
        print("tool failed:", diffbench)
        return 2
    a, b = load(mine), load(theirs)
    b = (STATUS.get(b[0], b[0]),) + b[1:]
    if a[:3] != b[:3]:
        print("status/size native %s gpgx %s" % (a[:3], b[:3]))
        return 1
    width = a[1]
    lines = sorted({i // 2 // width for i in range(0, len(a[3]), 2)
                    if a[3][i:i + 2] != b[3][i:i + 2]})
    if lines:
        print("pixels differ on %d lines, first %s" % (len(lines), lines[:8]))
        return 1
    print("identical status=%d %dx%d" % a[:3])
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
