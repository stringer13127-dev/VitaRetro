#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
bash "$ROOT/scripts/prepare_retroarch.sh"
: "${VITASDK:?VITASDK must point to VitaSDK}"
rm -rf "$ROOT/build"
cmake -S "$ROOT" -B "$ROOT/build" -DCMAKE_TOOLCHAIN_FILE="$VITASDK/share/vita.toolchain.cmake"
cmake --build "$ROOT/build" -j2
test -d "$ROOT/vendor/retroarch/package/retroarch-data"
(cd "$ROOT/vendor/retroarch/package" && zip -q -r "$ROOT/build/VitaRetro_0.1_DEV.vpk" retroarch-data)
cp "$ROOT/build/VitaRetro_0.1_DEV.vpk" "$ROOT/VitaRetro_0.1_DEV.vpk"
bash "$ROOT/scripts/verify_vpk.sh" "$ROOT/VitaRetro_0.1_DEV.vpk"
echo "Built: $ROOT/VitaRetro_0.1_DEV.vpk"
