"""Valida un informe del diffbench contra report.schema.json (subconjunto de JSON Schema).
Uso: validate_report.py informe.json [esquema.json]"""
import json, pathlib, sys

TYPES = {"object": dict, "array": list, "string": str, "integer": int, "number": (int, float), "null": type(None)}

def check(value, schema, path, errs):
    if "enum" in schema and value not in schema["enum"]:
        errs.append(f"{path}: {value!r} no está en {schema['enum']}")
        return
    t = schema.get("type")
    if t:
        names = t if isinstance(t, list) else [t]
        ok = any(isinstance(value, TYPES[n]) and not (n in ("integer", "number") and isinstance(value, bool)) for n in names)
        if not ok:
            errs.append(f"{path}: tipo {type(value).__name__} no es {names}")
            return
    if isinstance(value, dict):
        for k in schema.get("required", []):
            if k not in value: errs.append(f"{path}: falta {k}")
        props = schema.get("properties", {})
        if schema.get("additionalProperties") is False:
            for k in value:
                if k not in props: errs.append(f"{path}: campo no permitido {k}")
        for k, v in value.items():
            if k in props: check(v, props[k], f"{path}.{k}", errs)
    if isinstance(value, list):
        if "length" in schema and len(value) != schema["length"]:
            errs.append(f"{path}: longitud {len(value)} != {schema['length']}")
        for i, v in enumerate(value):
            if "items" in schema: check(v, schema["items"], f"{path}[{i}]", errs)

def main(argv):
    schema_path = pathlib.Path(argv[2]) if len(argv) > 2 else pathlib.Path(__file__).with_name("report.schema.json")
    doc = json.loads(pathlib.Path(argv[1]).read_text(encoding="utf-8"))
    errs = []
    check(doc, json.loads(schema_path.read_text(encoding="utf-8")), "$", errs)
    for e in errs: print("error:", e, file=sys.stderr)
    if not errs: print("informe válido")
    return 1 if errs else 0

if __name__ == "__main__": sys.exit(main(sys.argv))
