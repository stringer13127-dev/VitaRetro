#!/usr/bin/env bash
set -euo pipefail
VPK="${1:?Usage: verify_vpk.sh VitaRetro_0.1_DEV.vpk [payload.vrp]}"
PAYLOAD="${2:-$(dirname "$VPK")/VitaRetro_0.1_DEV_RetroArch.vrp}"
test "$(basename "$VPK")" = VitaRetro_0.1_DEV.vpk
test -s "$VPK"
test -s "$PAYLOAD"
unzip -tq "$VPK" >/dev/null
python3 - "$VPK" "$PAYLOAD" "$(dirname "$0")/../vendor/retroarch/payload_manifest.hpp" <<'PY'
import hashlib
import re
import struct
import sys
import zlib
import zipfile

vpk, payload, manifest = sys.argv[1:]
with zipfile.ZipFile(vpk) as package:
    names = set(package.namelist())
    required = {"eboot.bin", "sce_sys/param.sfo", "sce_sys/icon0.png",
                "sce_sys/livearea/contents/template.xml", "cacert.pem"}
    if not required <= names or len(names) > 30:
        raise SystemExit("Unexpected small VPK contents: " + str(names))
    if any(n.startswith("retroarch/") or n.startswith("retroarch-data/") for n in names):
        raise SystemExit("RetroArch files unexpectedly bundled in small VPK")
    if not all(package.getinfo(name).file_size for name in required):
        raise SystemExit("Empty required VPK file")

expected = re.search(r'VR_PAYLOAD_SHA256\[\] = "([0-9a-f]{64})"', open(manifest).read()).group(1)
with open(payload, "rb") as archive:
    digest = hashlib.file_digest(archive, "sha256").hexdigest()
    if digest != expected:
        raise SystemExit("Payload SHA-256 differs from VPK manifest")
    archive.seek(0)
    if archive.read(8) != b"VRPAY001":
        raise SystemExit("Invalid payload magic")
    count, = struct.unpack("<I", archive.read(4))
    if count < 100 or count > 100000:
        raise SystemExit("Suspicious payload entry count")
    seen = set()
    for _ in range(count):
        length, packed, raw, checksum = struct.unpack("<HIII", archive.read(14))
        path = archive.read(length).decode("utf-8")
        if path in seen or not (path.startswith("retroarch/") or path.startswith("data/")) or \
           any(part in ("", ".", "..") for part in path.split("/")):
            raise SystemExit(f"Invalid payload path: {path}")
        seen.add(path)
        data = zlib.decompress(archive.read(packed))
        if len(data) != raw or zlib.crc32(data) & 0xFFFFFFFF != checksum:
            raise SystemExit(f"Corrupt payload entry: {path}")
    if archive.read(1):
        raise SystemExit("Trailing payload bytes")
    for core in ("snes9x2005_plus", "genesis_plus_gx", "fceumm", "gambatte", "gpsp", "pcsx_rearmed"):
        if f"retroarch/{core}_libretro.self" not in seen:
            raise SystemExit(f"Official Vita core absent: {core}")
print(f"Verified small VPK and {count} official RetroArch files in separate payload; SHA-256 {digest}")
PY
