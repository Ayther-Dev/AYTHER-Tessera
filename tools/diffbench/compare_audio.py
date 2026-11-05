"""Sound chip write log (plane 2) of the differential bench (spec 008, TP-107).

Runs a ROM on the native machine (tessera_md_run) and on gpgx
(tessera_diffbench --libretro) and compares the AUDIO_WRITES region (id 11,
records of cycle:u32, addr:u16, data:u8, chip:u8) of the last frame; the entry
count comes from AUDIO_WRITE_COUNT (id 12). The native side writes the same
records with --audio-writes-out.

usage: compare_audio.py <md_run> <diffbench> <gpgx_core> <rom> <frames> <out_dir>
       [--show]         print both logs
       [--tolerance N]  cycles may differ by up to N master cycles (default 0)
exit:  0 identical, 1 different, 2 a tool failed
prints: audio_writes native=N gpgx=M differing=D max_cycle_diff=C

The cycle is in master cycles from the start of the frame (measured on gpgx,
TP-011.2); the sequence (chip, addr, data) must always be identical.
"""
import os
import struct
import subprocess
import sys

AUDIO_WRITES = 11
AUDIO_WRITE_COUNT = 12


def records(blob):
    return [struct.unpack_from("<IHBB", blob, i) for i in range(0, len(blob) - 7, 8)]


def main(argv):
    tolerance = 0
    if "--tolerance" in argv:
        at = argv.index("--tolerance")
        tolerance = int(argv[at + 1])
        del argv[at:at + 2]
    flags = [a for a in argv if a.startswith("--")]
    argv = [a for a in argv if not a.startswith("--")]
    md_run, diffbench, core, rom, frames, out_dir = argv[1:7]
    os.makedirs(out_dir, exist_ok=True)
    mine = os.path.join(out_dir, "native.audio")
    gpgx_log = os.path.join(out_dir, "gpgx.audio")
    gpgx_count = os.path.join(out_dir, "gpgx.count")
    wram = os.path.join(out_dir, "native.wram")
    runs = [
        [md_run, "--rom", rom, "--frames", frames, "--work-ram-out", wram,
         "--audio-writes-out", mine],
        [diffbench, "--libretro", core, "--rom", rom, "--frames", frames,
         "--dump-regions", "--region-out", "%d:%s" % (AUDIO_WRITES, gpgx_log)],
        [diffbench, "--libretro", core, "--rom", rom, "--frames", frames,
         "--dump-regions", "--region-out", "%d:%s" % (AUDIO_WRITE_COUNT, gpgx_count)],
    ]
    for command in runs:
        # --dump-regions exits 1 when a region is smaller than announced (25 of
        # 26 are complete in gpgx r4): only a missing output file is a failure.
        code = subprocess.run(command, capture_output=True).returncode
        if code != 0 and (command is runs[0] or code != 1):
            print("tool failed:", command[0], code)
            return 2
    for path in (mine, gpgx_log, gpgx_count):
        if not os.path.exists(path):
            print("tool failed: no output", path)
            return 2
    with open(mine, "rb") as f:
        native = records(f.read())
    with open(gpgx_count, "rb") as f:
        count = struct.unpack_from("<I", f.read(4))[0]
    with open(gpgx_log, "rb") as f:
        gpgx = records(f.read())[:count]

    if "--show" in flags:
        for name, log in (("native", native), ("gpgx", gpgx)):
            print(name)
            for r in log:
                print("  cycle=%7d addr=%04x data=%02x chip=%d" % r)
    differing = abs(len(native) - len(gpgx))
    worst = 0
    for a, b in zip(native, gpgx):
        worst = max(worst, abs(a[0] - b[0]))
        if a[1:] != b[1:] or abs(a[0] - b[0]) > tolerance:
            differing += 1
    print("audio_writes native=%d gpgx=%d differing=%d max_cycle_diff=%d"
          % (len(native), len(gpgx), differing, worst))
    return 0 if differing == 0 else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
