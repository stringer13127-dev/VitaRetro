#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
"$ROOT/scripts/prepare_retroarch.sh"
: "${VITASDK:?VITASDK must point to VitaSDK}"
rm -rf "$ROOT/build"
cmake -S "$ROOT" -B "$ROOT/build" -DCMAKE_TOOLCHAIN_FILE="$VITASDK/share/vita.toolchain.cmake"
cmake --build "$ROOT/build" -j2
cp "$ROOT/build/VitaRetro_0.1_DEV.vpk" "$ROOT/VitaRetro_0.1_DEV.vpk"
bash "$ROOT/scripts/verify_vpk.sh" "$ROOT/VitaRetro_0.1_DEV.vpk"
echo "Built: $ROOT/VitaRetro_0.1_DEV.vpk"
