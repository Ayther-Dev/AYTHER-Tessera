"""Framebuffer plane of the differential bench (spec 008, TP-105).

Runs a ROM on the native machine (tessera_md_run --frame-out) and on gpgx
(tessera_diffbench --libretro --frame-out) and compares the last frame pixel
by pixel. The oracle frame is RGB565 with a 1440 byte pitch; it is expanded to
8 bits per channel the way a host does it (v << 3 | v >> 2, v << 2 | v >> 4).
The native frame is RGBA8.

usage: compare_frame.py <md_run> <diffbench> <gpgx_core> <rom> <frames> <out_dir>
exit:  0 identical, 1 different, 2 a tool failed
"""
import os
import struct
import subprocess
import sys


def load(path):
    with open(path, "rb") as f:
        data = f.read()
    width, height, pitch = struct.unpack("<3I", data[:12])
    return width, height, pitch, data[12:]


def main(argv):
    md_run, diffbench, core, rom, frames, out_dir = argv[1:7]
    os.makedirs(out_dir, exist_ok=True)
    mine = os.path.join(out_dir, "native.frame")
    gpgx = os.path.join(out_dir, "gpgx.frame")
    runs = [
        [md_run, "--rom", rom, "--frames", frames, "--frame-out", mine,
         "--work-ram-out", os.path.join(out_dir, "native.wram")],
        [diffbench, "--libretro", core, "--rom", rom, "--frames", frames,
         "--frame-out", gpgx],
    ]
    for command in runs:
        if subprocess.run(command, capture_output=True).returncode != 0:
            print("tool failed:", command[0])
            return 2
    # PALETTE region (id 24): 256 RGB565 entries, shadow/normal/highlight.
    pal_native = os.path.join(out_dir, "native.palette")
    pal_gpgx = os.path.join(out_dir, "gpgx.palette")
    for command in (
        [md_run, "--rom", rom, "--frames", frames, "--palette-out", pal_native,
         "--work-ram-out", os.path.join(out_dir, "native.wram")],
        [diffbench, "--libretro", core, "--rom", rom, "--frames", frames,
         "--dump-regions", "--region-out", "24:" + pal_gpgx],
    ):
        subprocess.run(command, capture_output=True)  # dump-regions exits 1
    with open(pal_native, "rb") as f:
        pn = f.read()
    with open(pal_gpgx, "rb") as f:
        pg = f.read()
    palette_diff = [i for i in range(0, min(len(pn), len(pg)), 2)
                    if pn[i:i + 2] != pg[i:i + 2]]
    print("palette entries=%d differing=%d" % (len(pg) // 2, len(palette_diff)))
    palette_ok = len(pn) == len(pg) and not palette_diff
    # Work RAM word at $FF0000 (sprite flags accumulated by the ROM, if any).
    with open(os.path.join(out_dir, "native.wram"), "rb") as f:
        wn = f.read(2)
    gw_ram = os.path.join(out_dir, "gpgx.wram")
    subprocess.run([diffbench, "--libretro", core, "--rom", rom, "--frames",
                    frames, "--work-ram-out", gw_ram], capture_output=True)
    with open(gw_ram, "rb") as f:
        wg = f.read(2)
    wg = wg[::-1]  # gpgx serves work RAM with the bytes of each word swapped
    print("sprite flags native=%s gpgx=%s" % (wn.hex(), wg.hex()))
    flags_ok = wn == wg
    nw, nh, npitch, nd = load(mine)
    gw, gh, gpitch, gd = load(gpgx)
    if (nw, nh) != (gw, gh):
        print("different size native=%dx%d gpgx=%dx%d" % (nw, nh, gw, gh))
        return 1
    diff = []
    for y in range(nh):
        for x in range(nw):
            px = struct.unpack_from("<H", gd, y * gpitch + x * 2)[0]
            r, g, b = px >> 11, (px >> 5) & 63, px & 31
            ref = ((r << 3 | r >> 2), (g << 2 | g >> 4), (b << 3 | b >> 2))
            n = struct.unpack_from("<I", nd, y * npitch + x * 4)[0]
            got = (n & 255, (n >> 8) & 255, (n >> 16) & 255)
            if ref != got:
                diff.append((y, x, got, ref))
    print("frame %dx%d pixels=%d differing=%d" % (nw, nh, nw * nh, len(diff)))
    for y, x, got, ref in diff[:8]:
        print("  y=%d x=%d native=%s gpgx=%s" % (y, x, got, ref))
    if diff or not flags_ok or not palette_ok:
        return 1
    print("result=identical")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
