#!/usr/bin/env python3
"""Build a separate, verified deployment asset from official Vita payloads.

The small VPK contains only VitaRetro. This asset contains six official Vita
SELF executables and the official RetroArch Vita data in a streaming format.
Each file is independently zlib-compressed and CRC-checked at extraction.
"""
from pathlib import Path
import hashlib
import struct
import sys
import zlib

core_dir, data_dir, target, manifest = map(Path, sys.argv[1:5])
cores = ("snes9x2005_plus", "genesis_plus_gx", "fceumm", "gambatte", "gpsp", "pcsx_rearmed")
entries = [(f"retroarch/{name}_libretro.self", core_dir / f"{name}_libretro.self") for name in cores]
entries += [(f"data/{path.relative_to(data_dir).as_posix()}", path)
            for path in sorted(data_dir.rglob("*")) if path.is_file()]
if len(entries) < 100:
    raise SystemExit("Official RetroArch Vita data appears incomplete")

with target.open("wb") as output:
    output.write(b"VRPAY001" + struct.pack("<I", len(entries)))
    for name, path in entries:
        encoded = name.encode("utf-8")
        raw = path.read_bytes()
        compressed = zlib.compress(raw, 6)
        if len(encoded) > 500 or len(raw) > 0xFFFFFFFF or len(compressed) > 0xFFFFFFFF:
            raise SystemExit(f"Entry exceeds format limits: {name}")
        output.write(struct.pack("<HIII", len(encoded), len(compressed), len(raw),
                                 zlib.crc32(raw) & 0xFFFFFFFF))
        output.write(encoded)
        output.write(compressed)

sha256 = hashlib.file_digest(target.open("rb"), "sha256").hexdigest()
manifest.write_text(
    "#pragma once\n"
    'static const char VR_PAYLOAD_URL[] = "https://github.com/stringer13127-dev/VitaRetro/releases/download/0.1-dev/VitaRetro_0.1_DEV_RetroArch.vrp";\n'
    f'static const char VR_PAYLOAD_SHA256[] = "{sha256}";\n',
    encoding="utf-8")
print(f"Packed {len(entries)} official RetroArch files into {target.stat().st_size} bytes; SHA-256 {sha256}")
