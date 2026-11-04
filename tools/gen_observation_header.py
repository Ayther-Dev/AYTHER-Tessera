"""Genera include/tessera/tessera_observation.h y tests/contract/observation_abi_constants.inc
a partir del header de referencia ayther_api.h (spec 008, RF-3.0, TP-006.2).
Uso: gen_observation_header.py <ayther_api.h>"""
import pathlib, re, sys

PREFIXES = ("CAP_", "SUB_", "LAYOUT_", "SPR_OUT_", "REGION_ACCESS_", "REGION_WORD_SWAPPED_LE",
            "AUDIO_TRANSPORT_", "ABI_VERSION_1_", "ABI_VERSION_LATEST", "JOURNAL_MAX_EVENTS", "PALETTE_ENTRIES")

def main(ref):
    root = pathlib.Path(__file__).resolve().parent.parent
    text = pathlib.Path(ref).read_text(encoding="utf-8", errors="replace")
    macros = []
    for m in re.finditer(r"^#define AYTHER_(\w+)\s+(.+?)\s*(?:/\*.*)?$", text, re.M):
        name, val = m.group(1), m.group(2).rstrip("\ ").strip()
        if name.startswith(PREFIXES) and not name.endswith("_STR") and "AYTHER__STR" not in val:
            macros.append((name, val))
    body = text[text.index("enum ayther_region_id"):]
    body = body[body.index("{") + 1: body.index("};")]
    body = re.sub(r"/\*.*?\*/", "", body, flags=re.S)
    regions, n = [], 0
    for item in (s.strip() for s in body.split(",")):
        if not item: continue
        mm = re.fullmatch(r"AYTHER_REGION_(\w+)(?:\s*=\s*(\d+))?", item)
        n = int(mm.group(2)) if mm.group(2) else n + 1
        regions.append((mm.group(1), n))
    def conv(v): return re.sub(r"\bAYTHER_", "TESSERA_OBS_", v)
    h = ["// Constantes del contrato de observación ABI 1.10 (spec 008, RF-3.0).",
         "// GENERADO por tools/gen_observation_header.py desde ayther-abi-1.10-r4; no editar a mano.",
         "#ifndef TESSERA_OBSERVATION_H", "#define TESSERA_OBSERVATION_H", "", "#include <stdint.h>", ""]
    h += [f"#define TESSERA_OBS_{n} {conv(v)}" for n, v in macros]
    h += [""] + [f"#define TESSERA_OBS_REGION_{n} {i}" for n, i in regions]
    h += ["", "#endif /* TESSERA_OBSERVATION_H */", ""]
    (root / "include/tessera/tessera_observation.h").write_text("\n".join(h), encoding="utf-8", newline="\n")
    t = ["// GENERADO por tools/gen_observation_header.py; compara cada constante con la referencia.", ""]
    t += [f"static_assert(TESSERA_OBS_{n} == AYTHER_{n}, \"{n}\");" for n, _ in macros if not n.startswith("ABI_VERSION_LATEST")]
    t += [f"static_assert(TESSERA_OBS_REGION_{n} == AYTHER_REGION_{n}, \"REGION_{n}\");" for n, _ in regions]
    (root / "tests/contract/observation_abi_constants.inc").write_text("\n".join(t) + "\n", encoding="utf-8", newline="\n")
    print(f"{len(macros)} macros, {len(regions)} regiones")

if __name__ == "__main__": main(sys.argv[1])
