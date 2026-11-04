"""Genera include/tessera/tessera_observation.h y tests/contract/observation_abi_constants.inc
a partir del header de referencia ayther_api.h (spec 008, RF-3.0, TP-006.2).
Uso: gen_observation_header.py <ayther_api.h>"""
import pathlib, re, sys

PREFIXES = ("CAP_", "SUB_", "LAYOUT_", "SPR_OUT_", "REGION_ACCESS_", "REGION_WORD_SWAPPED_LE",
            "AUDIO_TRANSPORT_", "ABI_VERSION_1_", "ABI_VERSION_LATEST", "JOURNAL_MAX_EVENTS", "PALETTE_ENTRIES", "LINE_CELL_COLUMNS", "LINE_MAX", "MAX_")

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
    structs = []
    for m in re.finditer(r"typedef struct (ayther_\w+)\s*\{(.*?)\}\s*\1;", text, re.S):
        if m.group(1) == "ayther_interface_v1": continue
        b = re.sub(r"/\*.*?\*/", "", m.group(2), flags=re.S)
        b = re.sub(r"//[^\n]*|^[ \t]*#[^\n]*", "", b, flags=re.M)
        if "{" in b:
            print("omitida (union/anidada):", m.group(1)); continue
        members = []
        for decl in (d.strip() for d in b.split(";")):
            if not decl: continue
            tm = re.match(r"([A-Za-z_]\w*)\s+(.*)$", decl, re.S)
            for part in tm.group(2).split(","):
                am = re.fullmatch(r"(\w+)\s*(\[[^\]]+\])?", part.strip())
                if not am: raise SystemExit("no parseable: %s: %r" % (m.group(1), decl))
                members.append((tm.group(1), am.group(1), am.group(2) or ""))
        structs.append((m.group(1), members))
    def tn(t): return t.replace("ayther_", "tessera_obs_") if t.startswith("ayther_") else t
    h = ["// Constantes del contrato de observación ABI 1.10 (spec 008, RF-3.0).",
         "// GENERADO por tools/gen_observation_header.py desde ayther-abi-1.10-r4; no editar a mano.",
         "#ifndef TESSERA_OBSERVATION_H", "#define TESSERA_OBSERVATION_H", "", "#include <stdint.h>", ""]
    h += [f"#define TESSERA_OBS_{n} {conv(v)}" for n, v in macros]
    h += [""] + [f"#define TESSERA_OBS_REGION_{n} {i}" for n, i in regions]
    def tn(t): return t.replace("ayther_", "tessera_obs_") if t.startswith("ayther_") else t
    h += ["", "#ifdef __cplusplus", 'extern "C" {', "#endif", ""]
    for sname, members in structs:
        h.append(f"typedef struct {tn(sname)} {{")
        for ty, nm, ar in members: h.append(f"    {tn(ty)} {nm}{conv(ar)};")
        h.append(f"}} {tn(sname)};")
        h.append("")
    h += ["#ifdef __cplusplus", "}", "#endif", ""]
    h += ["#endif /* TESSERA_OBSERVATION_H */", ""]
    (root / "include/tessera/tessera_observation.h").write_text("\n".join(h), encoding="utf-8", newline="\n")
    t = ["// GENERADO por tools/gen_observation_header.py; compara cada constante con la referencia.", ""]
    t += [f"static_assert(TESSERA_OBS_{n} == AYTHER_{n}, \"{n}\");" for n, _ in macros if not n.startswith("ABI_VERSION_LATEST")]
    t += [f"static_assert(TESSERA_OBS_REGION_{n} == AYTHER_REGION_{n}, \"REGION_{n}\");" for n, _ in regions]
    for sname, members in structs:
        t.append(f"static_assert(sizeof({tn(sname)}) == sizeof({sname}), \"sizeof {sname}\");")
        for ty, nm, ar in members:
            t.append(f"static_assert(offsetof({tn(sname)}, {nm}) == offsetof({sname}, {nm}), \"{sname}.{nm}\");")
    for sname, members in structs:
        t.append(f'static_assert(sizeof({tn(sname)}) == sizeof({sname}), "sizeof {sname}");')
        for ty, nm, ar in members:
            t.append(f'static_assert(offsetof({tn(sname)}, {nm}) == offsetof({sname}, {nm}), "{sname}.{nm}");')
    (root / "tests/contract/observation_abi_constants.inc").write_text("\n".join(t) + "\n", encoding="utf-8", newline="\n")
    print(f"{len(macros)} macros, {len(regions)} regiones")

if __name__ == "__main__": main(sys.argv[1])
