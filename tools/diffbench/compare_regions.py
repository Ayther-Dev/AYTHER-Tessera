"""Memory regions of the differential bench (spec 008, TP-201.1, RF-3.1).

Runs a ROM on the native machine (tessera_md_run --region-out) and on gpgx
(tessera_diffbench --dump-regions --region-out) and compares, byte for byte,
VRAM (1), CRAM (2), VDP_REGS (3), VSRAM (4), SYSTEM (18) and Z80_RAM (26) as the ABI hands
them out, plus the work RAM (the legacy memory, id 0x1000 here).

usage: compare_regions.py <md_run> <diffbench> <gpgx_core> <rom> <frames> <out_dir>
                          [extra tessera_md_run arguments]
exit:  0 identical, 1 different, 2 a tool failed
"""
import os
import subprocess
import sys

REGIONS = [(1, "VRAM"), (2, "CRAM"), (3, "VDP_REGS"), (4, "VSRAM"), (18, "SYSTEM"),
           (24, "PALETTE"), (26, "Z80_RAM")]
WORK_RAM = 0x1000


def main(argv):
    md_run, diffbench, core, rom, frames, out_dir = argv[1:7]
    os.makedirs(out_dir, exist_ok=True)
    native, oracle = {}, {}
    for rid, _ in REGIONS + [(WORK_RAM, "WORK_RAM")]:
        native[rid] = os.path.join(out_dir, "native.%d" % rid)
        oracle[rid] = os.path.join(out_dir, "gpgx.%d" % rid)
        for path in (native[rid], oracle[rid]):
            if os.path.exists(path):
                os.remove(path)
    mine = [md_run, "--rom", rom, "--frames", frames, "--work-ram-out",
            os.path.join(out_dir, "unused.wram")] + argv[7:]
    for rid, _ in REGIONS:
        mine += ["--region-out", "%d:%s" % (rid, native[rid])]
    mine += ["--region-out", "%d:%s" % (WORK_RAM, native[WORK_RAM])]
    if subprocess.run(mine, capture_output=True).returncode != 0:
        print("tool failed:", md_run)
        return 2
    # The bench takes one --region-out per run. --dump-regions exits 1 when a
    # region is smaller than announced: only a missing output file is a failure.
    for rid, _ in REGIONS:
        theirs = [diffbench, "--libretro", core, "--rom", rom, "--frames", frames,
                  "--dump-regions", "--region-out", "%d:%s" % (rid, oracle[rid])]
        if subprocess.run(theirs, capture_output=True).returncode not in (0, 1):
            print("tool failed:", diffbench)
            return 2
    theirs = [diffbench, "--libretro", core, "--rom", rom, "--frames", frames,
              "--work-ram-out", oracle[WORK_RAM]]
    if subprocess.run(theirs, capture_output=True).returncode != 0:
        print("tool failed:", diffbench)
        return 2
    with open(oracle[WORK_RAM], "rb") as f:  # swapped words, as the bench writes
        pass
    status = 0
    for rid, name in REGIONS + [(WORK_RAM, "WORK_RAM")]:
        if not os.path.exists(native[rid]) or not os.path.exists(oracle[rid]):
            print("tool failed: no output for", name)
            return 2
        with open(native[rid], "rb") as f:
            a = f.read()
        with open(oracle[rid], "rb") as f:
            g = f.read()
        if len(a) != len(g):
            print("%s different sizes native=%d gpgx=%d" % (name, len(a), len(g)))
            status = 1
            continue
        diff = [i for i in range(len(a)) if a[i] != g[i]]
        print("%s bytes=%d differing=%d" % (name, len(a), len(diff)))
        for i in diff[:6]:
            print("  0x%05x native=%02x gpgx=%02x" % (i, a[i], g[i]))
        status |= bool(diff)
    return status


if __name__ == "__main__":
    sys.exit(main(sys.argv))
