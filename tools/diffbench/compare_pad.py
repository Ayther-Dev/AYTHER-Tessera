"""Pad ports of the differential bench (spec 008, TP-108).

Runs a `pad_latch` ROM on the native machine (tessera_md_run) and on gpgx
(tessera_diffbench --libretro) with the same constant pad masks (12 bits in
Mega Drive order: up down left right B C A start X Y Z mode) and compares the
bytes the ROM stored in work RAM. gpgx exposes the work RAM with the bytes of
every 16 bit word swapped, so that dump is swapped back.

usage: compare_pad.py <md_run> <diffbench> <gpgx_core> <rom> <frames> <out_dir>
       <mask1 hex> <mask2 hex> [--type 3|6] [--show N]
exit:  0 identical, 1 different, 2 a tool failed
prints: pad_latch bytes=N differing=D
"""
import os
import subprocess
import sys


def main(argv):
    flags = {}
    rest = []
    i = 1
    while i < len(argv):
        if argv[i] in ("--type", "--show") and i + 1 < len(argv):
            flags[argv[i]] = argv[i + 1]
            i += 2
        else:
            rest.append(argv[i])
            i += 1
    md_run, diffbench, core, rom, frames, out_dir, mask1, mask2 = rest[:8]
    os.makedirs(out_dir, exist_ok=True)
    mine = os.path.join(out_dir, "native.wram")
    theirs = os.path.join(out_dir, "gpgx.wram")
    runs = [
        [md_run, "--rom", rom, "--frames", frames, "--work-ram-out", mine,
         "--pad-mask", mask1, "--pad-mask2", mask2,
         "--pad-type", flags.get("--type", "3")],
        [diffbench, "--libretro", core, "--rom", rom, "--frames", frames,
         "--work-ram-out", theirs, "--pad-mask", mask1, "--pad-mask2", mask2]
        + (["--pad-device", "201"] if flags.get("--type") == "6" else []),
    ]
    for command in runs:
        if subprocess.run(command, capture_output=True).returncode != 0:
            print("tool failed:", command[0])
            return 2
    with open(mine, "rb") as f:
        a = f.read()
    with open(theirs, "rb") as f:
        b = bytearray(f.read())
    b[0::2], b[1::2] = b[1::2], b[0::2]
    # the ROM stores its reads from $FF0000: compare up to the last used byte
    used = max((i for i in range(len(a)) if a[i] or b[i]), default=-1) + 1
    differing = sum(1 for i in range(used) if a[i] != b[i])
    if "--show" in flags:
        n = int(flags["--show"])
        print("native", " ".join("%02x" % x for x in a[:n]))
        print("gpgx  ", " ".join("%02x" % x for x in b[:n]))
    print("pad_latch bytes=%d differing=%d" % (used, differing))
    return 0 if differing == 0 else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
