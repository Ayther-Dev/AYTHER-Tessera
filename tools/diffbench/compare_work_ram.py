"""Work RAM plane of the differential bench (spec 008, TP-101.9).

Runs a ROM on the native machine (tessera_md_run) and on gpgx
(tessera_diffbench --libretro) and compares the final work RAM. gpgx exposes
the 64 KiB with the bytes of every 16 bit word swapped (host order), so its
dump is swapped back before comparing.

usage: compare_work_ram.py <md_run> <diffbench> <gpgx_core> <rom> <frames> <out_dir>
       [region]   native region j|u|e (gpgx reads it from the ROM header)
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
        [md_run, "--rom", rom, "--frames", frames, "--work-ram-out", mine]
        + (["--region", argv[7]] if len(argv) > 7 else []),
        [diffbench, "--libretro", core, "--rom", rom, "--frames", frames,
         "--work-ram-out", gpgx],
    ]
    for command in runs:
        if subprocess.run(command, capture_output=True).returncode != 0:
            print("tool failed:", command[0])
            return 2
    with open(mine, "rb") as f:
        a = f.read()
    with open(gpgx, "rb") as f:
        g = bytearray(f.read())
    g[0::2], g[1::2] = g[1::2], g[0::2]
    if len(a) != len(g):
        print("different sizes", len(a), len(g))
        return 1
    diff = [i for i in range(len(a)) if a[i] != g[i]]
    print("work_ram bytes=%d differing=%d" % (len(a), len(diff)))
    for i in diff[:16]:
        print("  0x%05x native=%02x gpgx=%02x" % (i, a[i], g[i]))
    return 0 if not diff else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
