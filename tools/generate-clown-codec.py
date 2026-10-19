"""Manual, stdlib-only Clang AST audit/code generator; never a build dependency."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

COMMIT = "88ef45a6585556e2247dd26b6c937d6b9fb4a12d"
ROOTS = ["state", "m68k", "z80", "vdp.state", "fm.state", "psg.state",
         "controller_manager.state", "mega_cd.m68k", "mega_cd.cdc",
         "mega_cd.cdda.state", "mega_cd.pcm.state", "mega_cd.cdd"]
BOUNDS = {
    "state.external_ram.size": (0, 65536),
    "state.external_ram.data_size": (0, 3),
    "state.mega_cd.prg_ram.bank": (0, 3),
    "state.z80.bank": (0, 511),
    "vdp.state.plane_width_shift": (5, 7),
    "vdp.state.background_colour": (0, 63),
    "vdp.state.debug.forced_layer": (0, 3),
    "vdp.state.sprite_row_cache.rows[].total": (0, 32),
    "vdp.state.sprite_row_cache.rows[].sprites[].table_index": (0, 127),
    "vdp.state.sprite_row_cache.rows[].sprites[].y_in_sprite": (0, 63),
    "vdp.state.sprite_row_cache.rows[].sprites[].width": (0, 4),
    "vdp.state.sprite_row_cache.rows[].sprites[].height": (0, 4),
    "vdp.state.kdebug_buffer_index": (0, 254),
    "m68k.pending_interrupt": (0, 7),
    "mega_cd.m68k.pending_interrupt": (0, 7),
    "psg.state.latched_command.channel": (0, 3),
    "psg.state.tones[].attenuation": (0, 15),
    "psg.state.tones[].output_bit": (0, 1),
    "psg.state.noise.attenuation": (0, 15),
    "psg.state.noise.fake_output_bit": (0, 1),
    "psg.state.noise.real_output_bit": (0, 1),
    "psg.state.noise.frequency_mode": (0, 3),
    "psg.state.noise.type": (0, 1),
    "fm.state.lfo.frequency": (0, 7),
    "controller_manager.state.ea_multitap.selected_controller": (0, 7),
    "z80.register_mode": (0, 2),
    "fm.state.channels[].state.feedback_divisor": (2, 9),
    "fm.state.channels[].state.algorithm": (0, 7),
    "fm.state.channels[].state.phase_modulation_sensitivity": (0, 7),
    "fm.state.channels[].state.amplitude_modulation_shift": (0, 7),
    "fm.state.channels[].state.operators[].key_scale": (0, 3),
    "fm.state.channels[].state.operators[].countdown": (1, 3),
    "fm.state.channels[].state.operators[].attenuation": (0, 1023),
    "fm.state.channels[].state.operators[].total_level": (0, 1016),
    "fm.state.channels[].state.operators[].sustain_level": (0, 992),
    "fm.state.channels[].state.operators[].rates[]": (0, 31),
    "fm.state.channels[].state.operators[].phase.key_code": (0, 31),
    "fm.state.channels[].state.operators[].phase.detune": (0, 7),
    "fm.state.channels[].state.operators[].phase.multiplier": (1, 30),
    "fm.state.channels[].state.operators[].phase.f_number_and_block": (0, 16383),
    "mega_cd.pcm.state.current_channel": (0, 7),
    "mega_cd.pcm.state.current_wave_bank": (0, 15),
    "mega_cd.pcm.state.channels[].panning[]": (0, 15),
    "mega_cd.cdc.host_data_buffered_sector_index": (0, 4),
    "mega_cd.cdc.buffered_sectors_read_index": (0, 4),
    "mega_cd.cdc.buffered_sectors_write_index": (0, 4),
    "mega_cd.cdc.buffered_sectors_total": (0, 5),
}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--clang", required=True)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    source = args.source_root.resolve()
    repo = Path(__file__).resolve().parent.parent
    command = [args.clang, "-std=c11", "-DCC_USE_C99_INTEGERS", "-Xclang", "-ast-dump=json",
               "-fsyntax-only", "-x", "c", str(source / "source/clownmdemu.h")]
    process = subprocess.run(command, capture_output=True, text=True, check=True)
    ast = json.loads(process.stdout)
    records, enums = {}, {}

    def collect(node):
        if node.get("kind") == "RecordDecl" and node.get("completeDefinition") and node.get("name"):
            records[node["name"]] = node
        if node.get("kind") == "EnumDecl" and node.get("name"):
            values, previous = [], -1
            for child in node.get("inner", []):
                if child.get("kind") != "EnumConstantDecl":
                    continue
                value = next((int(item["value"]) for item in child.get("inner", []) if "value" in item), previous + 1)
                values.append(value)
                previous = value
            enums[node["name"]] = values
        for child in node.get("inner", []):
            collect(child)
    collect(ast)

    def fields(record):
        if record.get("tagUsed") == "union":
            raise RuntimeError("Union encountered; manual explicit codec required")
        return [child for child in record.get("inner", []) if child.get("kind") == "FieldDecl"]

    def resolve(field, parent):
        kind = field["type"]["qualType"]
        if field.get("isBitfield") or "*" in kind or "(" in kind and "unnamed at" not in kind:
            raise RuntimeError("Unsupported pointer/bitfield/function: " + str(field))
        dimensions = [int(size) for size in re.findall(r"\[(\d+)\]", kind)]
        base = re.sub(r"\[\d+\]", "", kind).strip()
        if "unnamed at" in base:
            offset = field["range"]["begin"]["offset"]
            matches = [child for child in parent.get("inner", []) if child.get("kind") == "RecordDecl"
                       and child.get("range", {}).get("begin", {}).get("offset") == offset]
            if len(matches) != 1:
                raise RuntimeError("Ambiguous anonymous record: " + base)
            return base, dimensions, matches[0]
        return base, dimensions, records.get(base.removeprefix("struct "))

    top_names = {field["name"] for field in fields(records["ClownMDEmu"])}
    if top_names != {"callbacks", "cartridge_buffer", "cartridge_buffer_length", "controller_manager", "m68k", "z80", "vdp", "fm", "psg", "mega_cd", "configuration", "state"}:
        raise RuntimeError("Top-level state changed; audit the coverage partition")
    lines, leaves, sections, payload_size = [], [], [], 0
    seen_bounds = set()

    def descend(record, expression, path, dimensions=(), depth=1):
        nonlocal payload_size
        if record is None:
            raise RuntimeError("Missing record: " + path)
        for field in fields(record):
            if not field.get("name"):
                raise RuntimeError("Unnamed field requires manual review")
            name = field["name"]
            base, shape, nested = resolve(field, record)
            field_expression, field_path = expression + "." + name, path + "." + name
            original_expression = field_expression
            generated_begin = len(lines)
            field_depth = depth
            for array_size in shape:
                variable = "element_" + str(field_depth)
                lines.append("    " * field_depth + f"static_assert(std::extent_v<std::remove_reference_t<decltype({field_expression})>> == {array_size});")
                lines.append("    " * field_depth + f"for (auto& {variable} : {field_expression}) {{")
                field_expression, field_path = variable, field_path + "[]"
                field_depth += 1
            all_dimensions = dimensions + tuple(shape)
            if nested:
                descend(nested, field_expression, field_path, all_dimensions, field_depth)
            else:
                match = re.fullmatch(r"cc_([us])(8|16|32)[lf]", base)
                enum_values = enums.get(base.removeprefix("enum "))
                if base == "cc_bool":
                    bits, lower, upper = 8, 0, 1
                elif base == "char":
                    bits, lower, upper = 8, -128, 127
                elif match:
                    bits = int(match[2])
                    lower, upper = (0, (1 << bits) - 1) if match[1] == "u" else (-(1 << (bits-1)), (1 << (bits-1))-1)
                elif enum_values:
                    if enum_values != list(range(min(enum_values), max(enum_values) + 1)):
                        raise RuntimeError("Noncontiguous enum needs an explicit membership test: " + base)
                    bits, lower, upper = 32, min(enum_values), max(enum_values)
                else:
                    raise RuntimeError("Unsupported scalar type: " + base)
                if field_path in BOUNDS:
                    lower, upper = BOUNDS[field_path]
                    seen_bounds.add(field_path)
                entry = dict(path=field_path, c_type=base, dimensions=all_dimensions, wire_bits=bits, minimum=lower, maximum=upper)
                if enum_values:
                    entry["enum_values"] = enum_values
                leaves.append(entry)
                count = 1
                for dimension in all_dimensions:
                    count *= dimension
                payload_size += count * (bits // 8)
                lines.append("    " * field_depth + f'ar.template integer<{bits}>({field_expression}, {lower}LL, {upper}LL, "{field_path}");')
            for unused in shape:
                field_depth -= 1
                lines.append("    " * field_depth + "}")
            if shape and base == "cc_u8l" and field_path not in BOUNDS:
                sizes = ", ".join(str(size) for size in shape)
                lines[generated_begin:] = ["    " * depth + f"ar.template octets<{sizes}>({original_expression});"]

    for tag, root in enumerate(ROOTS, 1):
        section_start = payload_size
        payload_size += 4
        record = records["ClownMDEmu"]
        for component in root.split("."):
            field = next(field for field in fields(record) if field["name"] == component)
            unused, shape, record = resolve(field, record)
            if shape:
                raise RuntimeError("Array root unsupported")
        lines.append(f"    ar.tag({tag}); // {root}")
        descend(record, "m." + root, root)
        sections.append(dict(tag=tag, path=root, payload_offset=section_start, wire_bytes=payload_size-section_start))
    if seen_bounds != set(BOUNDS):
        raise RuntimeError("Stale invariant paths: " + str(set(BOUNDS)-seen_bounds))
    inputs = {}
    for path in sorted(source.rglob("*")):
        if path.is_file() and path.suffix in (".h", ".c"):
            inputs[path.relative_to(source).as_posix()] = hashlib.sha256(path.read_bytes()).hexdigest()
    template_path = repo / "tools/generate-clown-codec.cpp.in"
    schema = dict(format_version=1, backend_commit=COMMIT, source_hashes=inputs, roots=ROOTS, sections=sections,
                  generator_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                  template_sha256=hashlib.sha256(template_path.read_bytes()).hexdigest(),
                  excluded_host_context=["callbacks", "cartridge_buffer", "cartridge_buffer_length", "all configuration fields"],
                  wire="section-tag-u32le then explicit fixed-width scalar fields in declaration order; arrays fixed by schema",
                  payload_wire_bytes=payload_size, leaf_declarations=leaves)
    canonical = json.dumps(schema, sort_keys=True, separators=(",", ":")).encode()
    identity = hashlib.sha256(canonical).hexdigest()
    schema["schema_sha256"] = identity
    template = template_path.read_text(encoding="utf-8")
    generated = template.replace("@@SCHEMA@@", identity).replace("@@COMMIT@@", COMMIT).replace("@@PAYLOAD_BYTES@@", str(payload_size)).replace("@@VISITOR@@", "\n".join(lines))
    outputs = {repo / "tests/backend_probe/clown_state_codec.cpp": generated,
               repo / "tools/generate-clown-codec.audit.json": json.dumps(schema, indent=2) + "\n"}
    for path, text in outputs.items():
        if args.check:
            if path.read_bytes() != text.encode():
                raise RuntimeError("Generated artifact differs: " + str(path))
        else:
            path.write_bytes(text.encode())
    print(f"CLOWN_CODEC_AUDIT schema={identity} roots={len(ROOTS)} leaf_declarations={len(leaves)} payload_bytes={payload_size} source_files={len(inputs)}")


if __name__ == "__main__":
    main()
