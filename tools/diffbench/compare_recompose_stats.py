"""recompose_stats of gpgx against the recorded fixture (spec 008, TP-205.3).

Each case of the fixture is a test ROM, a subscription mask (hex) and a stats
script that tessera_diffbench runs on gpgx (--stats-script, see main.cpp):
after frame N, in order, s/m recompose (single/multilayer, optional
"<flags hex>@<capacity>"), g reads the stats (optional out_size), p<region>=<hex>
writes a control, u<hex> sets the subscriptions, x saves and restores the
state. The fixture keeps what gpgx answered; the native side runs the same
cases through the public API in tests/integration/md_subscriptions_test.cpp.

usage: compare_recompose_stats.py <testrom> <diffbench> <gpgx_core> <fixture>
                                  <out_dir> [--update]
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


def run(testrom, diffbench, core, out_dir, rom, sub, script):
    rom_path = os.path.join(out_dir, rom + ".bin")
    if not os.path.exists(rom_path):
        if subprocess.run([testrom, rom, rom_path],
                          capture_output=True).returncode != 0:
            return None
    out = os.path.join(out_dir, "stats.txt")
    if os.path.exists(out):
        os.remove(out)
    subprocess.run([diffbench, "--libretro", core, "--rom", rom_path,
                    "--frames", FRAMES, "--sub-mask", sub,
                    "--stats-script", script, "--stats-out", out],
                   capture_output=True)
    if not os.path.exists(out):
        return None
    with open(out, encoding="ascii") as f:
        return [line.rstrip("\n") for line in f]


def main(argv):
    if len(argv) not in (6, 7):
        print(__doc__)
        return 2
    testrom, diffbench, core, fixture, out_dir = argv[1:6]
    update = len(argv) == 7 and argv[6] == "--update"
    os.makedirs(out_dir, exist_ok=True)
    recorded = cases(fixture)
    worst, rewritten = 0, []
    for header, expected in recorded:
        _, rom, sub, script = header.split(" ")
        got = run(testrom, diffbench, core, out_dir, rom, sub, script)
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
