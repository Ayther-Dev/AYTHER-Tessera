"""performance.md_budget (spec 008, TP-208.1, RNF-1, RNF-2): the native machine
(tessera_md_bench) and gpgx r4 (tessera_diffbench --bench-out) on the five
commercial games of the corpus, observation full and none, against the
provisional G1 targets of plan section 2.10.

usage: perf_budget.py <md_bench> <diffbench> <gpgx_core> <manifest.toml>
                      <corpus_dir> <out_dir> [--reps N] [--frames N]
                      [--evidence <file.json>]

Method of plan section 2.1: 3 repetitions x 600 frames with 60 of warm-up,
frames 0-600 from power-on, no input; the two modes alternate so that drift
hits both alike. The power overlay on AC is read from the registry and
recorded; the targets only hold in "Best performance" (ded574b5-...).
Prints one line per target and "result=ok" or "result=over_budget".
"""
import datetime
import hashlib
import json
import os
import statistics
import subprocess
import sys

GAMES = ["golden-axe-world", "sonic1-usa-eur", "streets-of-rage-2-usa",
         "phantasy-star-iv-usa", "gunstar-heroes-usa"]
BEST_PERFORMANCE = "ded574b5-45a0-4f42-8737-46345c09c238"
WARMUP = 60
MIB = 1024 * 1024


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


def read_entries(path):
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


def power_overlay():
    """ActiveOverlayAcPowerScheme, as plan section 2.1 checks it."""
    try:
        out = subprocess.run(
            ["reg", "query",
             r"HKLM\SYSTEM\CurrentControlSet\Control\Power\User\PowerSchemes",
             "/v", "ActiveOverlayAcPowerScheme"],
            capture_output=True, text=True).stdout
        for line in out.splitlines():
            if "ActiveOverlayAcPowerScheme" in line:
                return line.split()[-1]
    except OSError:
        pass
    return "unknown"


def bench(cmd, out):
    subprocess.run(cmd + ["--bench-out", out], check=True, capture_output=True)
    with open(out) as f:
        return json.load(f)


def main(argv):
    args = argv[1:]
    options = {"--reps": "3", "--frames": "600", "--evidence": None}
    for key in list(options):
        if key in args:
            at = args.index(key)
            options[key] = args[at + 1]
            del args[at:at + 2]
    if len(args) != 6:
        print(__doc__)
        return 2
    md_bench, diffbench, core, manifest, corpus_dir, out_dir = args
    reps, frames = int(options["--reps"]), int(options["--frames"])
    os.makedirs(out_dir, exist_ok=True)
    entries = {e["id"]: e["sha256"] for e in read_entries(manifest)
               if e.get("id") in GAMES}
    roms = find_roms(corpus_dir, set(entries.values()))
    overlay = power_overlay()
    result = {"date": datetime.date.today().isoformat(), "task": "TP-208.1",
              "method": "plan 2.1: per-frame step / retro_run, no input",
              "frames": frames, "warmup": WARMUP, "repetitions": reps,
              "power_overlay_ac": overlay,
              "best_performance": overlay == BEST_PERFORMANCE,
              "reference": "gpgx r4", "entries": {}}
    for game in GAMES:
        rom = roms.get(entries.get(game, ""))
        if rom is None:
            print(f"missing: {game}")
            return 1
        common = ["--frames", str(frames), "--warmup", str(WARMUP)]
        commands = {"tessera": [md_bench, "--rom", rom] + common,
                    "gpgx": [diffbench, "--libretro", core, "--rom", rom] +
                    common}
        entry = result["entries"][game] = {"sha256": entries[game]}
        for name, cmd in commands.items():
            runs = {"none": [], "full": []}
            for r in range(reps):
                for mode in ("none", "full"):
                    out = os.path.join(out_dir, f"{game}-{name}-{mode}-{r}.json")
                    runs[mode].append(bench(cmd + ["--observation", mode], out))
            entry[name] = {}
            for mode, rs in runs.items():
                m = {k: [x[k] for x in rs] for k in ("p50_us", "p95_us", "p99_us")}
                for k in ("savestate_bytes", "resident_state_bytes",
                          "session_live_bytes", "rom_bytes",
                          "observation_buffer_bytes", "process_peak_bytes",
                          "serialize_size"):
                    if k in rs[0]:
                        m[k] = max(x[k] for x in rs)
                entry[name][mode] = m

    checks = []

    def check(game, what, value, limit, unit):
        checks.append({"entry": game, "target": what, "value": round(value, 3),
                       "limit": round(limit, 3), "unit": unit,
                       "ok": value <= limit})

    for game, e in result["entries"].items():
        t, g = e["tessera"], e["gpgx"]
        med = lambda d, mode: statistics.median(d[mode]["p50_us"]) / 1000
        b, b0 = med(g, "full"), med(g, "none")
        check(game, "avance con observación, p50", med(t, "full"),
              max(2.0 * b, 5.0), "ms")
        check(game, "avance con observación, p99",
              max(t["full"]["p99_us"]) / 1000, 10.0, "ms")
        check(game, "avance sin observación, p50", med(t, "none"),
              max(2.0 * b0, 3.0), "ms")
        check(game, "estado residente", t["none"]["resident_state_bytes"] / MIB,
              4.0, "MiB")
        check(game, "savestate", t["none"]["savestate_bytes"] / MIB, 2.0, "MiB")
        check(game, "búferes de observación",
              t["full"]["observation_buffer_bytes"] / MIB, 4.0, "MiB")
        check(game, "pico de proceso con observación",
              t["full"]["process_peak_bytes"] / MIB, 48.0, "MiB")
        # TP-208.2: what the observation costs, full minus none (measured
        # in G1; G3 asks the cost of the observation when off <= 5 %).
        e["observation_cost_ms"] = round(med(t, "full") - med(t, "none"), 3)
        e["tessera_over_gpgx_none"] = round(med(t, "none") / b0, 2)
    result["checks"] = checks
    result["all_ok"] = all(c["ok"] for c in checks)
    text = json.dumps(result, indent=2, ensure_ascii=False)
    with open(os.path.join(out_dir, "perf.json"), "w", encoding="utf-8") as f:
        f.write(text + "\n")
    if options["--evidence"]:
        with open(options["--evidence"], "w", encoding="utf-8") as f:
            f.write(text + "\n")
    for c in checks:
        print(f"{'ok  ' if c['ok'] else 'OVER'} {c['entry']}: {c['target']} "
              f"{c['value']} <= {c['limit']} {c['unit']}")
    print(f"power_overlay_ac={overlay}")
    print("result=ok" if result["all_ok"] else "result=over_budget")
    return 0 if result["all_ok"] else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
