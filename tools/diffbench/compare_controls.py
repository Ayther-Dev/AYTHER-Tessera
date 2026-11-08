"""Render controls of the native machine against gpgx (spec 008, TP-205.2).

Writes LAYER_MASK and LAYER_DIM before frame 3 with RENDER_CONTROLS and
RECOMPOSITION subscribed, runs N frames on tessera_md_run (--controls) and on
gpgx (tessera_diffbench --poke) and compares what the controls touch: the
emitted frame, recompose_frame and the five layers of recompose_multilayer.
The oracle frame is RGB565, expanded to 8 bits per channel as compare_frame.py
does; the native one is RGBA8.

usage: compare_controls.py <md_run> <diffbench> <gpgx_core> <rom> <frames>
                           <out_dir> <mask hex>:<dim>[,<mask hex>:<dim>...]
       (or <layer_mask hex> <layer_dim> for one pair)
exit:  0 identical for every pair, 1 different, 2 a tool failed
"""
import os
import struct
import subprocess
import sys

SUBSCRIPTIONS = "24"  # RECOMPOSITION | RENDER_CONTROLS
FROM_FRAME = 3
# gpgx statuses as Tessera returns them (see compare_recompose.py).
STATUS = {-20: -5, -21: -5, -24: -5, -23: -1, -9: -28}


def read(path):
    with open(path, "rb") as f:
        return f.read()


def frame_rows(path, oracle):
    data = read(path)
    width, height, pitch = struct.unpack_from("<3I", data)
    rows = []
    for y in range(height):
        if oracle:
            row = struct.unpack_from("<%dH" % width, data, 12 + y * pitch)
            rows.append(tuple(
                ((p >> 11) << 3 | (p >> 11) >> 2) |
                ((((p >> 5) & 63) << 2 | ((p >> 5) & 63) >> 4) << 8) |
                (((p & 31) << 3 | (p & 31) >> 2) << 16) for p in row))
        else:
            row = struct.unpack_from("<%dI" % width, data, 12 + y * pitch)
            rows.append(tuple(p & 0xFFFFFF for p in row))
    return width, height, rows


def recomposed(path, layers):
    data = read(path)
    status, width, height = struct.unpack_from("<iII", data)
    n = width * height * 2
    return (STATUS.get(status, status), width, height,
            [data[12 + n * k:12 + n * (k + 1)] for k in range(layers)])


def differing_lines(a, b, width):
    return sorted({i // 2 // width for i in range(0, len(a), 2)
                   if a[i:i + 2] != b[i:i + 2]})


def main(argv):
    if len(argv) == 9:
        pairs = [(argv[7], argv[8])]
    elif len(argv) == 8:
        pairs = [tuple(p.split(":")) for p in argv[7].split(",")]
    else:
        print(__doc__)
        return 2
    worst = 0
    for mask, dim in pairs:
        status = compare(*argv[1:7], mask, dim)
        worst = max(worst, status)
    return worst


def compare(md_run, diffbench, core, rom, frames, out_dir, mask, dim):
    out_dir = os.path.join(out_dir, "%s-%s" % (mask, dim))
    os.makedirs(out_dir, exist_ok=True)
    out = {name: os.path.join(out_dir, name) for name in
           ("native.frame", "gpgx.frame", "native.rc", "gpgx.rc",
            "native.ml", "gpgx.ml")}
    if subprocess.run([md_run, "--rom", rom, "--frames", frames,
                       "--work-ram-out", os.path.join(out_dir, "unused.wram"),
                       "--controls", "%s:%s:%d" % (mask, dim, FROM_FRAME),
                       "--isolate-layers",
                       "--frame-out", out["native.frame"],
                       "--recompose-out", out["native.rc"],
                       "--multilayer-out", out["native.ml"]],
                      capture_output=True).returncode != 0:
        print("tool failed:", md_run)
        return 2
    run = subprocess.run(
        [diffbench, "--libretro", core, "--rom", rom, "--frames", frames,
         "--sub-mask", SUBSCRIPTIONS,
         "--poke", "5:0:%02x:%d,10:0:%02x:%d" % (int(mask, 16), FROM_FRAME,
                                                 int(dim), FROM_FRAME),
         "--frame-out", out["gpgx.frame"],
         "--recompose-out", out["gpgx.rc"],
         "--multilayer-out", out["gpgx.ml"]],
        capture_output=True, text=True)
    if run.returncode not in (0, 1) or "poke status=0" not in run.stdout:
        print("tool failed:", diffbench, run.stdout.strip())
        return 2
    problems = []
    nw, nh, nrows = frame_rows(out["native.frame"], False)
    gw, gh, grows = frame_rows(out["gpgx.frame"], True)
    if (nw, nh) != (gw, gh):
        problems.append("frame size native %dx%d gpgx %dx%d" %
                        (nw, nh, gw, gh))
    else:
        lines = [y for y in range(nh) if nrows[y] != grows[y]]
        if lines:
            problems.append("frame: %d lines, first %s" %
                            (len(lines), lines[:6]))
    for name, layers, labels in (("rc", 1, ("recompose",)),
                                 ("ml", 5, ("A", "B", "W", "S", "C"))):
        a = recomposed(out["native." + name], layers)
        b = recomposed(out["gpgx." + name], layers)
        if a[:3] != b[:3]:
            problems.append("%s status/size native %s gpgx %s" %
                            (name, a[:3], b[:3]))
            continue
        for label, la, lb in zip(labels, a[3], b[3]):
            lines = differing_lines(la, lb, a[1])
            if lines:
                problems.append("%s: %d lines, first %s" %
                                (label, len(lines), lines[:6]))
    if problems:
        print("different mask=%s dim=%s; %s" % (mask, dim,
                                                 "; ".join(problems)))
        return 1
    print("identical mask=%s dim=%s %dx%d" % (mask, dim, nw, nh))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
