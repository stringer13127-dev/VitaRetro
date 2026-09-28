#!/usr/bin/env bash
set -euo pipefail
VERSION="${RETROARCH_VERSION:-1.22.2}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEST="$ROOT/vendor/retroarch"
CACHE="$ROOT/.cache"
mkdir -p "$DEST/vpk" "$CACHE"
VPK="$CACHE/RetroArch-${VERSION}-vita.vpk"
URL="https://buildbot.libretro.com/stable/${VERSION}/playstation/vita/RetroArch.vpk"
DATA="$CACHE/RetroArch-${VERSION}-vita-data.7z"
DATA_URL="https://buildbot.libretro.com/stable/${VERSION}/playstation/vita/RetroArch_data.7z"
if [ ! -s "$VPK" ]; then
  echo "Downloading official RetroArch Vita ${VERSION}..."
  curl -fL --retry 3 "$URL" -o "$VPK"
fi
unzip -tq "$VPK" >/dev/null
rm -rf "$DEST/vpk"
mkdir -p "$DEST/vpk"
CORES=(snes9x2005_plus genesis_plus_gx fceumm gambatte gpsp pcsx_rearmed)
for core in "${CORES[@]}"; do
  entry="${core}_libretro.self"
  if ! unzip -Z1 "$VPK" | grep -Fx "$entry" >/dev/null; then
    echo "Required official Vita core missing: $entry" >&2
    exit 2
  fi
  unzip -p "$VPK" "$entry" > "$DEST/vpk/$entry"
  test -s "$DEST/vpk/$entry"
done
printf 'RetroArch Vita %s\ncores=%s\nsource=%s\n' "$VERSION" "${#CORES[@]}" "$URL" > "$DEST/BUILD_INFO.txt"
echo "Prepared ${#CORES[@]} official RetroArch Vita core executables."
if [ ! -s "$DATA" ]; then
  echo "Downloading official RetroArch Vita data ${VERSION}..."
  curl -fL --retry 3 "$DATA_URL" -o "$DATA"
fi
command -v 7z >/dev/null || { echo "7z is required for official Vita data" >&2; exit 2; }
rm -rf "$DEST/data" "$DEST/data-extracted"
mkdir -p "$DEST/data-extracted"
7z x -y "$DATA" -o"$DEST/data-extracted" >/dev/null
if [ -d "$DEST/data-extracted/retroarch" ]; then
  mv "$DEST/data-extracted/retroarch" "$DEST/data"
else
  mv "$DEST/data-extracted" "$DEST/data"
fi
rm -rf "$DEST/data-extracted"
test -n "$(find "$DEST/data" -type f -print -quit)"
echo "Prepared official RetroArch Vita data."
