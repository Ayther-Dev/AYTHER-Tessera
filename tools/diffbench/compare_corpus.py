"""Corpus titles on the framebuffer plane (spec 008, TP-105.11).

For every entry of the corpus manifest, finds the ROM by sha256 in the corpus
directory and compares the frame of the native machine against gpgx pixel by
pixel at several checkpoints of the first segment (frames 0 to 600 by default).
A checkpoint is a fresh cold boot run to that frame, so no input is applied.
A frame that is a single colour is reported: it proves nothing.

usage: compare_corpus.py <md_run> <diffbench> <gpgx_core> <manifest.toml>
                         <corpus_dir> <out_dir> [--planes framebuffer]
                         [--step N] [--entry ID]
exit:  0 no divergence, 1 divergence, 2 a tool failed or a ROM is missing
"""
import hashlib
import os
import struct
import subprocess
import sys

# D-3 exceptions covering a checkpoint are declared here as (entry id, frame):
# none so far.
DECLARED = set()


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
    if options["--planes"] != "framebuffer":
        print("only the framebuffer plane is implemented")
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
