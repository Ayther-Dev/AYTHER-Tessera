"""Corpus titles on the framebuffer plane (spec 008, TP-105.11).

For every entry of the corpus manifest, finds the ROM by sha256 in the corpus
directory and compares the frame of the native machine against gpgx pixel by
pixel at several checkpoints of the first segment (frames 0 to 600 by default).
A checkpoint is a fresh cold boot run to that frame, so no input is applied.
A frame that is a single colour is reported: it proves nothing.
The audio_writes plane (TP-107.7) compares, at the same checkpoints, the sound
chip write log of the last frame (compare_audio.py); a checkpoint with no write
is counted apart, since it proves nothing either.

usage: compare_corpus.py <md_run> <diffbench> <gpgx_core> <manifest.toml>
                         <corpus_dir> <out_dir>
                         [--planes framebuffer,audio_writes]
                         [--step N] [--entry ID]
exit:  0 no divergence, 1 divergence, 2 a tool failed or a ROM is missing
"""
import hashlib
import os
import struct
import subprocess
import sys

# Known framebuffer divergences, declared as (entry id, frame). They were
# found when the public ROMs entered the corpus (TP-004.3) and are identical
# on HEAD 6d29fe4, i.e. not caused by later work. 240p frame 60 is EXC-009
# of docs/parity-exceptions.md (approved 2026-11-09 for G1): the logo palette
# animation runs a few frames out of phase with gpgx; reviewed with the 68000
# DRAM refresh work before G2. Only the frames observed are declared, so a
# new diverging frame still fails the test.
# VDP FIFO Testing left the list on 2026-11-09 (TP-104.3: the bits a CRAM,
# VSRAM or 8-bit VRAM read takes from the FIFO; 9/9 like gpgx).
DECLARED = {("240p-test-suite-1.32", 60)}

# EXC-001..004 of docs/parity-exceptions.md (approved 2026-11-07): audio_writes
# cycle divergences from the 68000 DRAM refresh model, whole first segment.
# Valid only while no TP-209.3 identifier is affected.
DECLARED_AUDIO = {"golden-axe-world", "sonic1-usa-eur",
                  "streets-of-rage-2-usa", "phantasy-star-iv-usa"}


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


def read_entries(path):
    """The manifest subset used here: [[entry]] tables of id and sha256."""
    entries = []
    with open(path, encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if line == "[[entry]]":
                entries.append({})
            elif entries and line.startswith(("id", "sha256")) and "=" in line:
                key, value = line.split("=", 1)
                entries[-1][key.strip()] = value.strip().strip('"')
    return entries


def find_roms(corpus_dir, wanted):
    found = {}
    for root, _, names in os.walk(corpus_dir):
        for name in names:
            if not name.lower().endswith((".md", ".bin", ".gen", ".smd")):
                continue
            path = os.path.join(root, name)
            if os.path.getsize(path) < 64 * 1024:
                continue
            digest = sha256(path)
            if digest in wanted and digest not in found:
                found[digest] = path
    return found


def load(path):
    with open(path, "rb") as f:
        data = f.read()
    width, height, pitch = struct.unpack("<3I", data[:12])
    return width, height, pitch, data[12:]


def rgb565(px):
    r, g, b = px >> 11, (px >> 5) & 63, px & 31
    return (r << 3 | r >> 2, g << 2 | g >> 4, b << 3 | b >> 2)


def compare(md_run, diffbench, core, rom, frames, work):
    native = os.path.join(work, "native.frame")
    gpgx = os.path.join(work, "gpgx.frame")
    for command in (
        [md_run, "--rom", rom, "--frames", str(frames), "--frame-out", native,
         "--work-ram-out", os.path.join(work, "native.wram")],
        [diffbench, "--libretro", core, "--rom", rom, "--frames", str(frames),
         "--frame-out", gpgx],
    ):
        if subprocess.run(command, capture_output=True).returncode != 0:
            return None
    nw, nh, npitch, nd = load(native)
    gw, gh, gpitch, gd = load(gpgx)
    if (nw, nh) != (gw, gh):
        return {"size": ((nw, nh), (gw, gh)), "differing": -1, "colours": 0}
    differing, first, colours = 0, None, set()
    for y in range(nh):
        for x in range(nw):
            ref = rgb565(struct.unpack_from("<H", gd, y * gpitch + x * 2)[0])
            n = struct.unpack_from("<I", nd, y * npitch + x * 4)[0]
            mine = (n & 255, (n >> 8) & 255, (n >> 16) & 255)
            colours.add(ref)
            if mine != ref:
                differing += 1
                if first is None:
                    first = (x, y, mine, ref)
    return {"differing": differing, "first": first, "colours": len(colours)}


def compare_audio(md_run, diffbench, core, rom, frames, work):
    """(writes, differing) of the last frame at `frames`, or None on failure."""
    script = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                          "compare_audio.py")
    run = subprocess.run(
        [sys.executable, "-I", script, md_run, diffbench, core, rom,
         str(frames), os.path.join(work, "audio-%d" % frames)],
        capture_output=True, text=True)
    for line in run.stdout.splitlines():
        if line.startswith("audio_writes "):
            fields = dict(item.split("=") for item in line.split()[1:])
            return int(fields["native"]), int(fields["differing"])
    return None


def main(argv):
    args = [a for a in argv[1:]]
    options = {"--planes": "framebuffer", "--step": "60", "--entry": None,
               "--frames": "600"}
    positional = []
    while args:
        a = args.pop(0)
        if a in options:
            options[a] = args.pop(0)
        else:
            positional.append(a)
    md_run, diffbench, core, manifest, corpus_dir, out_dir = positional[:6]
    planes = set(options["--planes"].split(","))
    if not planes or not planes <= {"framebuffer", "audio_writes"}:
        print("planes implemented: framebuffer, audio_writes")
        return 2
    entries = read_entries(manifest)
    if options["--entry"]:
        entries = [e for e in entries if e["id"] == options["--entry"]]
    roms = find_roms(corpus_dir, {e["sha256"] for e in entries})
    step, last = int(options["--step"]), int(options["--frames"])
    checkpoints = sorted({1, 2, 5, 10, 20, 30, *range(step, last + 1, step), last})
    status = 0
    for entry in entries:
        rom = roms.get(entry["sha256"])
        if rom is None:
            print("%s: ROM with sha256 %s not found in the corpus directory"
                  % (entry["id"], entry["sha256"]))
            status = max(status, 2)
            continue
        work = os.path.join(out_dir, entry["id"])
        os.makedirs(work, exist_ok=True)
        bad, flat, shown = [], 0, 0
        if "audio_writes" in planes:
            heard, silent, wrong = 0, 0, []
            for frames in checkpoints:
                result = compare_audio(md_run, diffbench, core, rom, frames,
                                       work)
                if result is None:
                    print("%s: compare_audio failed at frame %d"
                          % (entry["id"], frames))
                    status = max(status, 2)
                    break
                writes, differing = result
                heard += writes > 0
                silent += writes == 0
                if differing and entry["id"] not in DECLARED_AUDIO:
                    wrong.append((frames, writes, differing))
            else:
                print("%s: audio_writes checkpoints=%d with_writes=%d "
                      "no_writes=%d divergences=%d"
                      % (entry["id"], len(checkpoints), heard, silent,
                         len(wrong)))
                for frames, writes, differing in wrong:
                    print("  frame %d: writes=%d differing=%d"
                          % (frames, writes, differing))
                if wrong:
                    status = max(status, 1)
        if "framebuffer" not in planes:
            continue
        for frames in checkpoints:
            result = compare(md_run, diffbench, core, rom, frames, work)
            if result is None:
                print("%s: a tool failed at frame %d" % (entry["id"], frames))
                status = max(status, 2)
                break
            if result["colours"] > 1:
                shown += 1
            else:
                flat += 1
            if result["differing"] and (entry["id"], frames) not in DECLARED:
                bad.append((frames, result))
        else:
            print("%s: checkpoints=%d with_picture=%d single_colour=%d "
                  "divergences=%d" % (entry["id"], len(checkpoints), shown,
                                      flat, len(bad)))
            for frames, result in bad:
                print("  frame %d: differing=%s first=%s"
                      % (frames, result["differing"], result.get("first")))
            if bad:
                status = max(status, 1)
    print("result=%s" % ("identical" if status == 0 else "different"))
    return status


if __name__ == "__main__":
    sys.exit(main(sys.argv))
