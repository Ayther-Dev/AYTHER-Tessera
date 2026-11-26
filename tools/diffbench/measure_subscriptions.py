"""Subscription matrix of gpgx (spec 008, TP-201.3), by black box.

For every mask (none, each single bit, all) and for 0 and 1 frames run, asks
the reference core for the status of every region read, the size of every
region (some change with the subscription) and the subscription state.

usage: measure_subscriptions.py <diffbench> <gpgx_core> <rom> <audio_rom> <out.json>
"""
import json
import re
import subprocess
import sys

MASKS = [0] + [1 << b for b in range(12)] + [0xFFF]


def probe(diffbench, core, rom, mask, frames):
    cmd = [diffbench, "--libretro", core, "--rom", rom, "--frames", str(frames),
           "--dump-regions", "--sub-mask", "%x" % mask]
    out = subprocess.run(cmd, capture_output=True, text=True).stdout
    regions = {}
    for m in re.finditer(r"region=(\d+) bytes=(\d+) access=(\d+) query=(-?\d+) "
                         r"elem=(\d+) cap=(\d+) ver=(\d+) legacy=(\d+) read=(-?\d+)", out):
        rid, byt, acc, q, el, cap, ver, leg, rd = map(int, m.groups())
        regions[rid] = {"bytes": byt, "access": acc, "query": q, "elem": el,
                        "cap": cap, "read": rd}
    state = {}
    for tag in ("after_set", "end"):
        m = re.search(tag + r" status=(-?\d+) size=(\d+) version=(\d+) supported=0x([0-9a-f]+) active=0x([0-9a-f]+) "
                      r"requested=0x([0-9a-f]+) activation_frame=(\d+)", out)
        if m:
            s, size, ver, sup, act, req, fr = m.groups()
            state[tag] = {"status": int(s), "struct_size": int(size), "version": int(ver),
                          "supported": int(sup, 16), "active": int(act, 16),
                          "requested": int(req, 16), "activation_frame": int(fr)}
    m = re.search(r"set_status=(-?\d+)", out)
    poll = re.search(r"audio_poll status=(-?\d+) count=(\d+) transport_status=(-?\d+)", out)
    return {"set_status": int(m.group(1)) if m else None, "state": state, "regions": regions,
            "audio_poll": {"status": int(poll.group(1)), "events": int(poll.group(2)) > 0,
                           "transport_status": int(poll.group(3))} if poll else None}


def main(argv):
    diffbench, core, rom, audio_rom, out_path = argv[1:6]
    matrix = {}
    for mask in MASKS:
        for frames in (0, 1):
            matrix["mask=0x%x,frames=%d" % (mask, frames)] = probe(diffbench, core, rom, mask, frames)
    # a mask with a bit the core does not have
    matrix["mask=0x1000,frames=0"] = probe(diffbench, core, rom, 0x1000, 0)
    # the audio transport: needs a ROM that writes to the sound chips
    for mask in (0, 0x40, 0x10, 0x50, 0xFFF):
        matrix["audio,mask=0x%x,frames=20" % mask] = probe(diffbench, core, audio_rom, mask, 20)
    with open(out_path, "w") as f:
        json.dump(matrix, f, indent=1, sort_keys=True)
    # the same facts as plain lines, for the C++ test (tests/integration/
    # md_subscriptions_test.cpp): status of every region read, the state after
    # the request and after the frames, and the audio transport.
    lines = []
    for key in sorted(matrix):
        case = matrix[key]
        if key.startswith("audio,"):
            mask = int(key.split("mask=0x")[1].split(",")[0], 16)
            poll = case["audio_poll"]
            lines.append("audio %x %d %d %d" % (mask, poll["status"], int(poll["events"]),
                                                poll["transport_status"]))
            continue
        mask = int(key.split("mask=0x")[1].split(",")[0], 16)
        frames = int(key.split("frames=")[1])
        st = case["state"]
        lines.append("set %x %d %d" % (mask, frames, case["set_status"]))
        lines.append("state %x %d after_set %d %d %d" % (
            mask, frames, st["after_set"]["active"], st["after_set"]["requested"],
            st["after_set"]["activation_frame"]))
        lines.append("state %x %d end %d %d %d" % (
            mask, frames, st["end"]["active"], st["end"]["requested"],
            st["end"]["activation_frame"]))
        for rid in sorted(case["regions"], key=int):
            lines.append("read %x %d %s %d" % (mask, frames, rid, case["regions"][rid]["read"]))
    with open(out_path.replace(".json", ".txt"), "w") as f:
        f.write("\n".join(lines) + "\n")
    print("measured", len(matrix), "cases")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
