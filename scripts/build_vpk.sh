#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
command -v pngquant >/dev/null || { echo "pngquant is required for Vita LiveArea images" >&2; exit 1; }
for image in \
  "$ROOT/assets/sce_sys/icon0.png" \
  "$ROOT/assets/sce_sys/livearea/contents/bg.png" \
  "$ROOT/assets/sce_sys/livearea/contents/startup.png"; do
  pngquant --ext=.png --force -- "$image"
done
bash "$ROOT/scripts/prepare_retroarch.sh"
: "${VITASDK:?VITASDK must point to VitaSDK}"
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
