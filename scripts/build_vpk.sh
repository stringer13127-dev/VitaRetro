#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
bash "$ROOT/scripts/prepare_retroarch.sh"
: "${VITASDK:?VITASDK must point to VitaSDK}"
cp /etc/ssl/certs/ca-certificates.crt "$ROOT/vendor/cacert.pem"
python3 "$ROOT/scripts/pack_retroarch_payload.py" \
  "$ROOT/vendor/retroarch/vpk" \
  "$ROOT/vendor/retroarch/package/retroarch-data" \
  "$ROOT/VitaRetro_0.1_DEV_RetroArch.vrp" \
  "$ROOT/vendor/retroarch/payload_manifest.hpp"
rm -rf "$ROOT/build"
cmake -S "$ROOT" -B "$ROOT/build" -DCMAKE_TOOLCHAIN_FILE="$VITASDK/share/vita.toolchain.cmake"
cmake --build "$ROOT/build" -j2
cp "$ROOT/build/VitaRetro_0.1_DEV.vpk" "$ROOT/VitaRetro_0.1_DEV.vpk"
bash "$ROOT/scripts/verify_vpk.sh" "$ROOT/VitaRetro_0.1_DEV.vpk"
echo "Built: $ROOT/VitaRetro_0.1_DEV.vpk"
