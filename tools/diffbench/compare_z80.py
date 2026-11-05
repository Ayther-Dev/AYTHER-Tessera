"""Work RAM and Z80 RAM planes of the differential bench (spec 008, TP-103).

Runs a ROM on the native machine (tessera_md_run) and on gpgx
(tessera_diffbench --libretro) and compares the final work RAM and the 8 KiB
of Z80 RAM (observation region 26, Z80_RAM). gpgx exposes the work RAM with the
bytes of every 16 bit word swapped (host order), so that dump is swapped back;
the Z80 RAM comes in natural order.

usage: compare_z80.py <md_run> <diffbench> <gpgx_core> <rom> <frames> <out_dir>
       [poke]   <offset hex>:<bytes hex>:<frame> written into Z80_RAM through
                the control write of both sides (TP-103.4)
exit:  0 identical, 1 different, 2 a tool failed
prints: work_ram bytes=N differing=D / z80_ram bytes=N differing=D
"""
import os
import subprocess
import sys

Z80_RAM_REGION = 26


def main(argv):
    md_run, diffbench, core, rom, frames, out_dir = argv[1:7]
    os.makedirs(out_dir, exist_ok=True)
    mine_wram = os.path.join(out_dir, "native.wram")
    mine_z80 = os.path.join(out_dir, "native.z80")
    gpgx_wram = os.path.join(out_dir, "gpgx.wram")
    gpgx_z80 = os.path.join(out_dir, "gpgx.z80")
    runs = [
        [md_run, "--rom", rom, "--frames", frames, "--work-ram-out", mine_wram,
         "--z80-ram-out", mine_z80],
        [diffbench, "--libretro", core, "--rom", rom, "--frames", frames,
         "--work-ram-out", gpgx_wram, "--dump-regions", "--region-out",
         "%d:%s" % (Z80_RAM_REGION, gpgx_z80)],
    ]
    if len(argv) > 7:
        offset, data, frame = argv[7].split(":")
        runs[0] += ["--poke-z80", argv[7]]
        runs[1] += ["--poke", "%d:%s:%s:%s" % (Z80_RAM_REGION, offset, data, frame)]
    for command in runs:
        # --dump-regions exits 1 when a region is smaller than announced (25 of
        # 26 are complete in gpgx r4): only a missing output file is a failure.
        code = subprocess.run(command, capture_output=True).returncode
        if code != 0 and (command is runs[0] or code != 1):
            print("tool failed:", command[0], code)
            return 2
    if not os.path.exists(gpgx_z80) or not os.path.exists(gpgx_wram):
        print("tool failed: gpgx produced no dump")
        return 2

    def load(path):
        with open(path, "rb") as f:
            return f.read()

    wram_a, wram_g = load(mine_wram), bytearray(load(gpgx_wram))
    wram_g[0::2], wram_g[1::2] = wram_g[1::2], wram_g[0::2]
    z80_a, z80_g = load(mine_z80), load(gpgx_z80)
    if len(wram_a) != len(wram_g) or len(z80_a) != len(z80_g):
        print("different sizes", len(wram_a), len(wram_g), len(z80_a), len(z80_g))
        return 1
    wram_diff = [i for i in range(len(wram_a)) if wram_a[i] != wram_g[i]]
    z80_diff = [i for i in range(len(z80_a)) if z80_a[i] != z80_g[i]]
    print("work_ram bytes=%d differing=%d" % (len(wram_a), len(wram_diff)))
    for i in wram_diff[:16]:
        print("  work 0x%05x native=%02x gpgx=%02x" % (i, wram_a[i], wram_g[i]))
    print("z80_ram bytes=%d differing=%d" % (len(z80_a), len(z80_diff)))
    for i in z80_diff[:16]:
        print("  z80 0x%04x native=%02x gpgx=%02x" % (i, z80_a[i], z80_g[i]))
    return 0 if not wram_diff and not z80_diff else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
