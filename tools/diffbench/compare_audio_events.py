"""Audio event transport of the differential bench (spec 008, TP-107.6).

Runs a ROM on the native machine (tessera_md_run --audio-events-out) and on
gpgx (tessera_diffbench --libretro --audio-events-out, which drains
poll_audio_events after every frame) and compares the 80 byte
ayther_audio_event_v1 records. Only the transport is compared: RAW_WRITE and
FRAME events. The events gpgx derives from the writes (NOTE_ON, DAC_START,
PITCH, VOLUME, ...) belong to the audio probe and are skipped.

usage: compare_audio_events.py <md_run> <diffbench> <gpgx_core> <rom> <frames>
                               <out_dir> [--tolerance N] [--show]
exit:  0 identical, 1 different, 2 a tool failed
prints: audio_events native=N gpgx=M differing=D max_cycle_diff=C
"""
import os
import struct
import subprocess
import sys

RAW_WRITE, FRAME = 0, 10
LAYOUT = "<QIIBBBBIII"  # t_global t_frame t_cycles source type channel schema
                        # group reg data; 48 more bytes (the voice arm)
STATS = "<8I"


def events(path):
    with open(path, "rb") as f:
        blob = f.read()
    out = []
    for at in range(0, len(blob) - 79, 80):
        out.append(struct.unpack_from(LAYOUT, blob, at))
    return out


def stats(path):
    with open(path, "rb") as f:
        blob = f.read()
    return struct.unpack_from("<i", blob, 0)[0], struct.unpack_from(STATS, blob, 4)


def main(argv):
    tolerance = 0
    if "--tolerance" in argv:
        at = argv.index("--tolerance")
        tolerance = int(argv[at + 1])
        del argv[at:at + 2]
    show = "--show" in argv
    argv = [a for a in argv if a != "--show"]
    md_run, diffbench, core, rom, frames, out_dir = argv[1:7]
    os.makedirs(out_dir, exist_ok=True)
    mine = os.path.join(out_dir, "native.events")
    gpgx = os.path.join(out_dir, "gpgx.events")
    wram = os.path.join(out_dir, "native.wram")
    for path in (mine, mine + ".stats", gpgx, gpgx + ".stats"):
        if os.path.exists(path):
            os.remove(path)
    runs = [
        [md_run, "--rom", rom, "--frames", frames, "--work-ram-out", wram,
         "--audio-events-out", mine],
        [diffbench, "--libretro", core, "--rom", rom, "--frames", frames,
         "--audio-events-out", gpgx],
    ]
    for command in runs:
        if subprocess.run(command, capture_output=True).returncode != 0:
            print("tool failed:", command[0])
            return 2
    for path in (mine, mine + ".stats", gpgx, gpgx + ".stats"):
        if not os.path.exists(path):
            print("tool failed: no output", path)
            return 2
    keep = lambda e: e[4] in (RAW_WRITE, FRAME)
    native = [e for e in events(mine) if keep(e)]
    oracle = [e for e in events(gpgx) if keep(e)]
    if show:
        for name, log in (("native", native), ("gpgx", oracle)):
            print(name)
            for e in log:
                print("  g=%d f=%d c=%d src=%d type=%d ch=%d schema=%d grp=%d "
                      "reg=%x data=%x" % e)
    differing = abs(len(native) - len(oracle))
    worst = 0
    for a, b in zip(native, oracle):
        # t_global, t_frame, t_cycles, then source type channel schema group
        # reg data
        worst = max(worst, abs(a[0] - b[0]), abs(a[2] - b[2]))
        if (a[1], *a[3:]) != (b[1], *b[3:]) or abs(a[0] - b[0]) > tolerance \
                or abs(a[2] - b[2]) > tolerance:
            differing += 1
    print("audio_events native=%d gpgx=%d differing=%d max_cycle_diff=%d"
          % (len(native), len(oracle), differing, worst))
    # The static part of the stats must agree: struct_size, version, event_size,
    # capacity, flags (the counters depend on how the observer drains).
    (ns, nv), (gs, gv) = stats(mine + ".stats"), stats(gpgx + ".stats")
    same = ns == gs == 0 and nv[:4] == gv[:4] and nv[7] == gv[7] and nv[6] == gv[6] == 0
    print("transport_stats native=%s gpgx=%s %s" % (nv, gv, "same" if same else "DIFFERENT"))
    return 0 if differing == 0 and same else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
