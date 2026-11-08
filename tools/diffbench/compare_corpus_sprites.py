"""Parsed sprites of the corpus games (spec 008, TP-202.1/.2).

Runs every corpus game from power on, with no input (title screens and
attract modes, up to frame 600: from about frame 1000 on Sonic 1 and others
already differ in the picture, see integration.md_corpus), on the native machine (tessera_md_run) and on gpgx
(tessera_diffbench) and, at several checkpoints, compares byte for byte
PARSED_SPRITES (13), its count (14) and SPRITE_OUTCOME (25).

With --timeline the pad timeline of the game (tools/timelines/<id>.timeline
next to the manifest) is played too. Today the two machines drift apart a
frame after a button press in Golden Axe (frame 605 of its timeline; found
while writing this, not diagnosed), so that mode is for investigation only.

usage: compare_corpus_sprites.py <md_run> <diffbench> <gpgx_core> <manifest>
                                 <corpus_dir> <out_dir> [--entry ID]
                                 [--checkpoints N] [--timeline]
exit:  0 identical, 1 different, 2 a tool failed or a ROM is missing
"""
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import compare_corpus  # noqa: E402

REGIONS = [(13, "PARSED_SPRITES"), (14, "PARSED_SPRITE_COUNT"),
           (25, "SPRITE_OUTCOME")]
FPS = 59.9227
# Mega Drive pad order of the bench: up down left right B C A start X Y Z mode
BUTTONS = {"up": 0, "down": 1, "left": 2, "right": 3, "b": 4, "c": 5, "a": 6,
           "start": 7, "x": 8, "y": 9, "z": 10, "mode": 11}


def duration(text):
    text = text.lower()
    if text.endswith("ms"):
        return int(round(float(text[:-2]) * FPS / 1000.0))
    if text.endswith("s"):
        return int(round(float(text[:-1]) * FPS))
    if text.endswith("f"):
        return int(float(text[:-1]))
    raise ValueError("duration: " + text)


def mask_of(names):
    mask = 0
    for name in names.lower().split("+"):
        mask |= 1 << BUTTONS[name]
    return mask


def compile_timeline(path):
    """(pad-script text, frames): the held mask changes become <frame>:<mask>:0."""
    held, frame, changes = 0, 0, []

    def hold_for(mask, frames):
        nonlocal frame
        changes.append((frame, held | mask))
        frame += frames
        changes.append((frame, held))

    with open(path, encoding="utf-8") as f:
        for line in f:
            line = line.split("#", 1)[0].split()
            if not line:
                continue
            key = line[0].lower()
            if key == "start":
                continue
            if key == "wait":
                frame += duration(line[1])
            elif key in ("press", "hold"):
                hold_for(mask_of(line[1]),
                         duration(line[2]) if len(line) > 2 else 4)
            elif key == "down":
                held |= mask_of(line[1])
                changes.append((frame, held))
            elif key == "up":
                held &= ~mask_of(line[1])
                changes.append((frame, held))
    last = {}
    for at, mask in changes:  # the last change of a frame wins
        last[at] = mask
    script = ",".join("%d:%x:0" % (at, last[at]) for at in sorted(last))
    return script, frame


def main(argv):
    args = argv[1:]
    options = {"--entry": None, "--checkpoints": "5"}
    timeline = False
    positional = []
    while args:
        a = args.pop(0)
        if a == "--timeline":
            timeline = True
        elif a in options:
            options[a] = args.pop(0)
        else:
            positional.append(a)
    md_run, diffbench, core, manifest, corpus_dir, out_dir = positional[:6]
    entries = compare_corpus.read_entries(manifest)
    if options["--entry"]:
        entries = [e for e in entries if e["id"] == options["--entry"]]
    timelines = os.path.join(os.path.dirname(os.path.abspath(manifest)),
                             "timelines")
    if timeline:
        entries = [e for e in entries if os.path.exists(
            os.path.join(timelines, e["id"] + ".timeline"))]
    roms = compare_corpus.find_roms(corpus_dir, {e["sha256"] for e in entries})
    os.makedirs(out_dir, exist_ok=True)
    status = 0
    for entry in entries:
        rom = roms.get(entry["sha256"])
        if rom is None:
            print("%s: ROM not found" % entry["id"])
            return 2
        count = int(options["--checkpoints"])
        if timeline:
            script, total = compile_timeline(
                os.path.join(timelines, entry["id"] + ".timeline"))
            points = [max(1, total * k // count) for k in range(1, count + 1)]
        else:
            script, points = None, [120 * k for k in range(1, count + 1)]
        pad = ["--pad-script", script] if script else []
        different, with_sprites = [], 0
        for frames in points:
            work = os.path.join(out_dir, "%s-%d" % (entry["id"], frames))
            os.makedirs(work, exist_ok=True)
            mine = [md_run, "--rom", rom, "--frames", str(frames),
                    "--work-ram-out", os.path.join(work, "unused.wram"),
                    ] + pad
            for rid, _ in REGIONS:
                mine += ["--region-out", "%d:%s" % (rid, os.path.join(
                    work, "native.%d" % rid))]
            if subprocess.run(mine, capture_output=True).returncode != 0:
                print("tool failed:", md_run)
                return 2
            for rid, name in REGIONS:
                theirs = [diffbench, "--libretro", core, "--rom", rom,
                          "--frames", str(frames)] + pad + [
                          "--sub-mask", "2", "--dump-regions", "--region-out",
                          "%d:%s" % (rid, os.path.join(work, "gpgx.%d" % rid))]
                if subprocess.run(theirs, capture_output=True).returncode \
                        not in (0, 1):
                    print("tool failed:", diffbench)
                    return 2
                with open(os.path.join(work, "native.%d" % rid), "rb") as f:
                    a = f.read()
                with open(os.path.join(work, "gpgx.%d" % rid), "rb") as f:
                    b = f.read()
                if rid == 14 and a[0] > 0:
                    with_sprites += 1
                if a != b:
                    different.append((frames, name))
        print("%s: checkpoints=%d with_sprites=%d divergences=%d" %
              (entry["id"], len(points), with_sprites, len(different)))
        for frames, name in different:
            print("  frame %d: %s differs" % (frames, name))
        status |= bool(different)
    print("result=%s" % ("identical" if status == 0 else "different"))
    return status


if __name__ == "__main__":
    sys.exit(main(sys.argv))
