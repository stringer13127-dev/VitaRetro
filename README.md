# VitaRetro 0.1 DEV

VitaRetro is a PS Vita homebrew prototype. The app version remains **0.1 DEV** until installation and startup are confirmed on a real Vita. The code keeps Vita-specific execution in `retroarch_manager` so future platform backends can be added without treating a Vita SELF as a desktop dynamic core.

## Current prototype

- Native 960 × 544 display with five URL slots. Choose an empty source, scan the displayed QR from a phone on the same Wi-Fi, paste **one global site URL**, and submit. VitaRetro serves the pairing form locally on port 28781 with a random session path and saves the URL on the Vita under `ux0:/data/VitaRetro/sources.txt`. Square lets you replace an existing source URL. This pairing flow still needs a real Vita and network test.
- A saved URL currently identifies the source; it does **not** turn an arbitrary website into a searchable game catalog. Site-specific provider adapters, network parsing, filters, and automatic downloads are not implemented. The UI states this directly rather than displaying fake results.
- Triangle from the source picker opens an explicit local demo test. X looks for a user-provided `ux0:/data/VitaRetro/roms/demo.sfc` or `demo.md`; neither is bundled.
- A Vita-specific resolver for NES, SNES, GB/GBC, GBA, Mega Drive and PS1. `.bin` is ambiguous: a catalog provider must supply the actual platform for reliable PS1/Genesis selection. PS1 compatibility can require a user-supplied BIOS.

## Cloud build

The GitHub Actions workflow uses the official `vitasdk/vitasdk:2026.08` image. It downloads official stable **RetroArch Vita 1.22.2** from Libretro at build time and selects six Vita SELF executables plus the official Vita data. No commercial games or BIOS are bundled.

The output is a **small** `VitaRetro_0.1_DEV.vpk` (title ID `VRET00001`) plus a separate `VitaRetro_0.1_DEV_RetroArch.vrp` deployment asset. The app bundle contains only VitaRetro. CI verifies the VPK and every compressed payload entry, including six official Vita cores, then publishes both assets in the `0.1-dev` prerelease.

After installing the small VPK, press **Select**, then **X** on the Vita to deploy RetroArch. VitaRetro downloads the release payload through the Vita's native HTTPS stack, checks its pinned SHA-256, then writes the six official cores to its installed app directory and the data under `ux0:/data/retroarch/`. It preserves existing settings/data and shows download, verification, and extraction progress. This needs enough free storage and an internet connection on the Vita. If Vita HTTPS fails, the same payload can be downloaded on Android and copied to `ux0:/data/VitaRetro/VitaRetro_0.1_DEV_RetroArch.vrp` by VitaShell FTP; the in-app button verifies and deploys this local file. Download and SELF launch on real Vita hardware remain unverified.

The command for an equivalent VitaSDK environment is `bash scripts/build_vpk.sh`; this is handled in GitHub Actions for Android-only users.

## Next work

Test the lightweight VPK installation/startup, the in-app RetroArch download/deployment, and local phone pairing on a real Vita while keeping version 0.1 DEV. Then build actual source adapters, catalog navigation, platform metadata, legal content handling, and launch validation. No arbitrary site's catalog can be inferred safely from its URL alone.

Libretro Vita installation documentation: https://docs.libretro.com/guides/install-psv/
RetroArch source and licenses: https://github.com/libretro/RetroArch
