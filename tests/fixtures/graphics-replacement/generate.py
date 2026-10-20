"""Generate the reproducible, signed FX-03 graphics-pack seed locally."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path
import struct
import subprocess
import zlib


HERE = Path(__file__).resolve().parent
ROM = HERE / "fx03_rom.md"
PACK_TOOL = Path("C:/ayther/AYTHER-Engine/build/windows-native/bin/make_test_pack.exe")


def png(rgba: tuple[int, int, int, int]) -> bytes:
    def chunk(kind: bytes, body: bytes) -> bytes:
        return struct.pack(">I", len(body)) + kind + body + struct.pack(
            ">I", zlib.crc32(kind + body) & 0xFFFFFFFF
        )

    scanline = bytes((0,)) + bytes(rgba) * 8
    image = scanline * 8
    return (
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", struct.pack(">IIBBBBB", 8, 8, 8, 6, 0, 0, 0))
        + chunk(b"IDAT", zlib.compress(image, level=9))
        + chunk(b"IEND", b"")
    )


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> None:
    if not PACK_TOOL.is_file():
        raise SystemExit(f"Build the local Engine make_test_pack target first: {PACK_TOOL}")
    first = HERE / "assets/first.png"
    second = HERE / "assets/second.png"
    first.parent.mkdir(parents=True, exist_ok=True)
    first.write_bytes(png((255, 0, 255, 255)))
    second.write_bytes(png((0, 255, 255, 255)))
    pack = HERE / "fx03.ay"
    trust = HERE / "trust.toml"
    result = subprocess.run(
        [str(PACK_TOOL), str(pack), str(trust), "auto",
         str(ROM), "--fx03", str(first), str(second)],
        check=True, capture_output=True, text=True,
    )
    parsed = dict(line.split("=", 1) for line in result.stdout.splitlines() if "=" in line)
    game_id = f"crc32:{zlib.crc32(ROM.read_bytes()) & 0xFFFFFFFF:08x}"
    if parsed["game_id"] != game_id:
        raise SystemExit("Engine's generated ROM and pack game ID disagree")
    metadata = {
        "status": "overlap ROM and pack; presentation oracles pending",
        "game_id": game_id,
        "tile_hash": parsed["tile_hash"],
        "rom_sha256": sha256(ROM),
        "pack_sha256": sha256(pack),
        "trust_sha256": sha256(trust),
        "assets": {"first.png": sha256(first), "second.png": sha256(second)},
        "catalog_winner": "assets/first.png",
        "runtime_override_winner": "assets/second.png",
        "catalog_expected_rgba": [255, 0, 255, 255],
        "override_expected_rgba": [0, 255, 255, 255],
        "candidate_masks": [
            {"x": 32, "y": 32, "width": 8, "height": 8},
            {"x": 96, "y": 64, "width": 8, "height": 8},
        ],
        "original_pixel_oracles": [
            {"x": 97, "y": 68, "rgba": [255, 0, 0, 255], "winner": "plane_a_priority_1"},
            {"x": 101, "y": 68, "rgba": [255, 0, 0, 255], "winner": "plane_a_over_sprite_0"},
            {"x": 105, "y": 68, "rgba": [0, 255, 0, 255], "winner": "sat_slot_0_over_slot_1"},
            {"x": 109, "y": 68, "rgba": [0, 0, 255, 255], "winner": "sat_slot_1"},
        ],
        "presentation_oracles": {
            "catalog": [
                {"x": 36, "y": 36, "rgba": [255, 0, 255, 255]},
                {"x": 100, "y": 68, "rgba": [255, 0, 255, 255]},
                {"x": 105, "y": 68, "rgba": [0, 255, 0, 255]},
                {"x": 109, "y": 68, "rgba": [0, 0, 255, 255]},
            ],
            "override": [
                {"x": 36, "y": 36, "rgba": [0, 255, 255, 255]},
                {"x": 100, "y": 68, "rgba": [0, 255, 255, 255]},
                {"x": 105, "y": 68, "rgba": [0, 255, 0, 255]},
                {"x": 109, "y": 68, "rgba": [0, 0, 255, 255]},
            ],
            "outside_candidate_masks": "byte-identical to original RGBA frame",
        },
        "license": "CC0-1.0",
        "author": "AYTHER Tessera",
    }
    (HERE / "manifest.json").write_text(
        json.dumps(metadata, indent=2) + "\n", encoding="utf-8"
    )


if __name__ == "__main__":
    main()
