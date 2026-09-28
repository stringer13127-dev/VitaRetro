#!/usr/bin/env bash
set -euo pipefail
VPK="${1:?Usage: verify_vpk.sh VitaRetro_0.1_DEV.vpk}"
test "$(basename "$VPK")" = VitaRetro_0.1_DEV.vpk
test -s "$VPK"
unzip -tq "$VPK" >/dev/null
for entry in eboot.bin sce_sys/param.sfo sce_sys/icon0.png \
  sce_sys/livearea/contents/template.xml \
  retroarch/snes9x2005_plus_libretro.self \
  retroarch/genesis_plus_gx_libretro.self \
  retroarch/fceumm_libretro.self \
  retroarch/gambatte_libretro.self \
  retroarch/gpsp_libretro.self \
  retroarch/pcsx_rearmed_libretro.self; do
  if ! unzip -Z1 "$VPK" | grep -Fx "$entry" >/dev/null; then
    echo "Missing VPK entry: $entry" >&2
    exit 1
  fi
  test "$(unzip -p "$VPK" "$entry" | wc -c)" -gt 0
done
if ! unzip -Z1 "$VPK" | grep -E "^retroarch-data/.+" >/dev/null; then
  echo "Official RetroArch data missing from VPK" >&2
  exit 1
fi
echo "Verified VPK contents: executable, SFO, LiveArea, six official Vita cores."
