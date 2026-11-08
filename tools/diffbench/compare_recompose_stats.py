"""recompose_stats and frame_delta of gpgx against a recorded fixture (spec
008, TP-205.3 and TP-205.4).

Each case of the fixture is a test ROM, a subscription mask (hex), a stats
script and optionally the frames to run (5 if absent); tessera_diffbench runs
the script on gpgx (--stats-script, see main.cpp):
after frame N, in order, s/m recompose (single/multilayer, optional
"<flags hex>@<capacity>"), g reads the stats (optional out_size), p<region>=<hex>
writes a control, u<hex> sets the subscriptions, x saves and restores the
state, S/L save a state and restore it later, d[out_size] polls the frame
delta, D<generation>[,out_size] asks for the delta since a generation (TP-205.4;
"D-<n>" counts back from the last poll), h reads FRAME_HASH (TP-206.1; H also
prints the host's own hashes, for diagnosis). A case rom "fx02" is the FX-02
synthetic ROM given with --fx02. The fixture keeps what gpgx
answered; the native side runs the same cases through the public API in
tests/integration/md_subscriptions_test.cpp.

usage: compare_recompose_stats.py <testrom> <diffbench> <gpgx_core> <fixture>
                                  <out_dir> [--update] [--fx02 <rom>]
exit:  0 gpgx answers as recorded (or fixture rewritten), 1 different,
       2 a tool failed
"""
import os
import subprocess
import sys

FRAMES = "5"


def cases(path):
    result, current = [], None
    with open(path, encoding="ascii") as f:
        for line in f:
            line = line.rstrip("\n")
            if not line or line.startswith("#"):
                continue
            if line.startswith("case "):
                current = (line, [])
                result.append(current)
            elif line != "end":
                current[1].append(line)
    return result


FX02 = None  # path of the FX-02 synthetic ROM (--fx02), the case rom "fx02"


def run(testrom, diffbench, core, out_dir, rom, sub, script, frames):
    rom_path = os.path.join(out_dir, rom + ".bin")
    if rom == "fx02":
        if FX02 is None:
            return None
        rom_path = FX02
    elif not os.path.exists(rom_path):
        if subprocess.run([testrom, rom, rom_path],
                          capture_output=True).returncode != 0:
            return None
    out = os.path.join(out_dir, "stats.txt")
    if os.path.exists(out):
        os.remove(out)
    subprocess.run([diffbench, "--libretro", core, "--rom", rom_path,
                    "--frames", frames, "--sub-mask", sub,
                    "--stats-script", script, "--stats-out", out],
                   capture_output=True)
    if not os.path.exists(out):
        return None
    with open(out, encoding="ascii") as f:
        return [line.rstrip("\n") for line in f]


def main(argv):
    global FX02
    argv = list(argv)
    if "--fx02" in argv:
        at = argv.index("--fx02")
        FX02 = argv[at + 1]
        del argv[at:at + 2]
    if len(argv) not in (6, 7):
        print(__doc__)
        return 2
    testrom, diffbench, core, fixture, out_dir = argv[1:6]
    update = len(argv) == 7 and argv[6] == "--update"
    os.makedirs(out_dir, exist_ok=True)
    recorded = cases(fixture)
    worst, rewritten = 0, []
    for header, expected in recorded:
        _, rom, sub, script, *frames = header.split(" ")
        got = run(testrom, diffbench, core, out_dir, rom, sub, script,
                  frames[0] if frames else FRAMES)
        if got is None:
            print("tool failed:", header)
            return 2
        rewritten.append((header, got))
        if got != expected and not update:
            worst = 1
            print("different:", header)
            for a, b in zip(expected + [""] * len(got), got + [""] * len(expected)):
                if a != b:
                    print("  fixture:", a)
                    print("  gpgx:   ", b)
                    break
    if update:
        with open(fixture, encoding="ascii") as f:
            comments = [l for l in f if l.startswith("#")]
        with open(fixture, "w", encoding="ascii", newline="\n") as f:
            f.writelines(comments)
            for header, lines in rewritten:
                f.write("\n" + header + "\n")
                for line in lines:
                    f.write(line + "\n")
                f.write("end\n")
        print("fixture rewritten: %d cases" % len(rewritten))
        return 0
    if worst == 0:
        print("identical: %d cases" % len(recorded))
    return worst


if __name__ == "__main__":
    sys.exit(main(sys.argv))
