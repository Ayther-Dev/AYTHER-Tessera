"""RESET button of the differential bench (spec 008, TP-110.4, RF-4.4).

Black box only: a probe ROM counts its starts in work RAM ($FF0100) and in Z80
RAM ($A00010) after asking for the Z80 bus. The ROM runs `frames` frames on the
native machine (tessera_md_run --soft-reset-at, or --hard-reset-at with
`hard`) and on gpgx (retro_reset through tessera_diffbench --reset-at), the
RESET button pressed before frame `at`. Work RAM and Z80 RAM are compared.
Nothing of gpgx is read but what the host observes.

usage: compare_reset.py <md_run> <diffbench> <gpgx_core> <frames> <at> <out_dir>
       [hard] [rom]   `rom` replaces the probe (boot counts then read 0)
exit:  0 identical, 1 different, 2 a tool failed
"""
import os
import struct
import subprocess
import sys

Z80_RAM_REGION = 26


def probe_rom():
    code = [
        0x33FC, 0x0100, 0x00A1, 0x1200,  # move.w #$100,$A11200 (Z80 reset off)
        0x33FC, 0x0100, 0x00A1, 0x1100,  # move.w #$100,$A11100 (request)
        0x0839, 0x0000, 0x00A1, 0x1100,  # wait: btst #0,$A11100
        0x66F6,                          # bne wait
        0x5239, 0x00A0, 0x0010,          # addq.b #1,$A00010 (Z80 RAM)
        0x52B9, 0x00FF, 0x0100,          # addq.l #1,$FF0100 (work RAM)
        0x33FC, 0x0000, 0x00A1, 0x1100,  # release the bus
        0x60FE,                          # bra *
    ]
    rom = bytearray(0x400)
    rom[0:8] = struct.pack(">II", 0xFFFE00, 0x200)
    rom[0x100:0x110] = b"SEGA MEGA DRIVE "
    rom[0x120:0x130] = b"AYTHER RESET PRB"
    rom[0x1A4:0x1A8] = struct.pack(">I", len(rom) - 1)
    rom[0x1F0:0x200] = b"U" + b" " * 15
    for i, word in enumerate(code):
        rom[0x200 + 2 * i:0x202 + 2 * i] = struct.pack(">H", word)
    return bytes(rom)


def probe_io_rom():
    """Stores the I/O control registers it finds at start in $FF0110, then sets
    them: after a RESET they show whether the console kept or cleared them."""
    code = [
        0x23F9, 0x00A1, 0x0008, 0x00FF, 0x0110,  # move.l $A10008,$FF0110
        0x13FC, 0x0040, 0x00A1, 0x0009,          # move.b #$40,$A10009
        0x13FC, 0x0040, 0x00A1, 0x000B,          # move.b #$40,$A1000B
        0x60FE,                                  # bra *
    ]
    rom = bytearray(probe_rom())
    rom[0x120:0x130] = b"AYTHER RESET IO "
    rom[0x200:0x300] = bytes(0x100)
    for i, word in enumerate(code):
        rom[0x200 + 2 * i:0x202 + 2 * i] = struct.pack(">H", word)
    return bytes(rom)


def main(argv):
    md_run, diffbench, core, frames, at, out_dir = argv[1:7]
    hard = len(argv) > 7 and argv[7] == "hard"
    os.makedirs(out_dir, exist_ok=True)
    rom = os.path.join(out_dir, "reset_probe.md")
    if len(argv) > 8 and argv[8] == "@io":
        with open(rom, "wb") as f:
            f.write(probe_io_rom())
    elif len(argv) > 8:
        rom = argv[8]
    else:
        with open(rom, "wb") as f:
            f.write(probe_rom())
    mine_w = os.path.join(out_dir, "native.wram")
    mine_z = os.path.join(out_dir, "native.z80")
    gpgx_w = os.path.join(out_dir, "gpgx.wram")
    gpgx_z = os.path.join(out_dir, "gpgx.z80")
    for path in (mine_w, mine_z, gpgx_w, gpgx_z):
        if os.path.exists(path):
            os.remove(path)
    runs = [
        [md_run, "--rom", rom, "--frames", frames, "--work-ram-out", mine_w,
         "--z80-ram-out", mine_z,
         "--hard-reset-at" if hard else "--soft-reset-at", at],
        [diffbench, "--libretro", core, "--rom", rom, "--frames", frames,
         "--reset-at", at, "--work-ram-out", gpgx_w, "--dump-regions",
         "--region-out", "%d:%s" % (Z80_RAM_REGION, gpgx_z)],
    ]
    for command in runs:
        # --dump-regions exits 1 when a region is smaller than announced.
        code = subprocess.run(command, capture_output=True).returncode
        if code != 0 and (command is runs[0] or code != 1):
            print("tool failed:", command[0], code)
            return 2
    for path in (mine_w, mine_z, gpgx_w, gpgx_z):
        if not os.path.exists(path):
            print("tool failed: no output", path)
            return 2

    def load(path):
        with open(path, "rb") as f:
            return bytearray(f.read())

    wram_a, wram_g = load(mine_w), load(gpgx_w)
    wram_g[0::2], wram_g[1::2] = wram_g[1::2], wram_g[0::2]
    z80_a, z80_g = load(mine_z), load(gpgx_z)
    status = 0
    for name, a, g in (("work_ram", wram_a, wram_g), ("z80_ram", z80_a, z80_g)):
        if len(a) != len(g):
            print(name, "different sizes", len(a), len(g))
            status = 1
            continue
        diff = [i for i in range(len(a)) if a[i] != g[i]]
        print("%s bytes=%d differing=%d" % (name, len(a), len(diff)))
        for i in diff[:8]:
            print("  0x%05x native=%02x gpgx=%02x" % (i, a[i], g[i]))
        status |= bool(diff)
    print("boots work_ram native=%d gpgx=%d" % (
        struct.unpack(">I", bytes(wram_a[0x100:0x104]))[0],
        struct.unpack(">I", bytes(wram_g[0x100:0x104]))[0]))
    print("io control seen at last start: native=%s gpgx=%s" % (
        bytes(wram_a[0x110:0x114]).hex(), bytes(wram_g[0x110:0x114]).hex()))
    print("boots z80_ram  native=%d gpgx=%d" % (z80_a[0x10], z80_g[0x10]))
    return status


if __name__ == "__main__":
    sys.exit(main(sys.argv))
