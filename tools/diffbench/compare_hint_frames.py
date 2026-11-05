"""HINT per frame plane of the differential bench (spec 008, TP-106.2).

The hint_counter ROM stores, at $FF0100 + n, how many horizontal interrupts
happened between vertical interrupt n and n + 1. The first frames depend on
how the CPU boots (gpgx starts the 68000 at line 159 of a frame that begins at
line 192, with no interrupt on that first frame), so the whole list has to be
equal as well as the steady state: every entry but the last one, which the
benchmark may or may not have reached.

usage: compare_hint_frames.py <md_run> <diffbench> <gpgx_core> <rom> <frames> <out_dir>
exit:  0 same steady value, 1 different, 2 a tool failed
"""
import os
import subprocess
import sys


def main(argv):
    md_run, diffbench, core, rom, frames, out_dir = argv[1:7]
    count = int(frames)
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
    steady = []
    whole = []
    for path in (mine, gpgx):
        with open(path, "rb") as f:
            data = f.read()
        # gpgx hands the work RAM with every 16 bit word byte swapped
        if path == gpgx:
            swapped = bytearray(data)
            swapped[0::2], swapped[1::2] = data[1::2], data[0::2]
            data = bytes(swapped)
        steady.append(sorted(set(data[0x100 + 3:0x100 + count - 3])))
        whole.append(list(data[0x100:0x100 + count - 1]))
    ok = steady[0] == steady[1] and len(steady[0]) == 1 and whole[0] == whole[1]
    print("steady native=%s gpgx=%s whole native=%s gpgx=%s match=%d" %
          (steady[0], steady[1], whole[0][:4], whole[1][:4], int(ok)))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
