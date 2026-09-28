# VitaRetro 0.1 DEV — Work handoff

## Mission
Take this existing VitaRetro project and carry it all the way to a REAL, installable PS Vita `.vpk` artifact, working entirely in cloud/browser/GitHub infrastructure. The user is operating only from an Android smartphone and should not be asked to install a compiler, VitaSDK, Git, or any development tool locally.

Do not restart the project from scratch. Preserve version `0.1 DEV` until the user confirms that the VPK installs and starts correctly.

## Non-negotiable UX rules
- PS Vita is the first and only build target for now, but architecture must remain portable later to Android phone, Android TV / Fire TV, and Nintendo Switch.
- The normal user experience must be automated: detect platform, resolve emulator/core, ensure runtime/dependencies where feasible, prepare content, and launch without requiring manual core selection.
- Advanced/manual core selection may exist but must not be necessary for normal use.
- UI organization should be inspired by the supplied StreamFlix screenshots: compact left sidebar, dense horizontal rows, ~6 covers visible per row on Vita 960x544, source-oriented browsing, and `Recherche & filtres`.
- Never present mockups as real screenshots. Only call an image a screenshot after the compiled app has actually rendered that UI.

## RetroArch requirement
- Use the official PS Vita RetroArch build/data from Libretro, not a desktop/Android build.
- Verify the current official stable Vita build and the official Vita installation/packaging model before integrating.
- Vita RetroArch uses statically linked cores / Vita-specific executables, so do not assume desktop-style dynamic `.so` core loading.
- Build a robust `system -> backend/core` resolver.
- Initial target systems should include at least: NES, SNES, Game Boy/Game Boy Color, GBA, Mega Drive/Genesis, and PS1 where technically supported.
- Validate the actual Vita mechanism required to launch content through those executables. Do not merely write plausible-looking command lines; verify them against Vita/Libretro behavior.
- Do not bundle or fetch copyrighted commercial ROMs. For automated smoke tests use no ROM, open/homebrew test content, or user-provided legal dumps only.

## Source/provider architecture
Keep source/provider parsing separate from UI and platform backends. A source is a site/provider adapter, not just a browser page. Global search keeps results grouped by source; do not deduplicate identical titles by default.

Per-source navigation should support, when the provider exposes them:
- Accueil (site's featured/current games, not the site's visual design)
- Nouveautés / recent additions
- Categories / genres
- Platforms
- Games
- Emulators
- Homebrews
- Search & filters

Search & filters must work even with an empty text query. Global search applies filters across enabled sources while keeping source sections separate.

## Cloud-only build requirement
The user has no PC. Use GitHub + GitHub Actions (or another genuinely free cloud build path) as the compiler machine.

Goal:
1. Put/import this project in a public GitHub repository named `VitaRetro` if the user's GitHub session allows it. If account authorization or a destructive/security confirmation is required, ask only for that confirmation and continue immediately afterward.
2. Add/fix a reproducible GitHub Actions workflow that installs/uses VitaSDK in the cloud.
3. Fetch the official PS Vita RetroArch payload from an official Libretro source during the build if redistribution/bundling constraints make that preferable; otherwise vendor only what is technically/licensing-wise appropriate.
4. Compile VitaRetro.
5. Package an actual installable VPK named exactly `VitaRetro_0.1_DEV.vpk`.
6. Upload it as a GitHub Actions artifact.
7. If the build fails, inspect logs, fix source/workflow, commit, rerun, and iterate until the artifact is genuinely produced.
8. Do not report success merely because source code exists. Success means an Actions run completed and the `.vpk` artifact exists and is downloadable.

## Validation before declaring success
At minimum verify from build output:
- `eboot.bin` was produced by the Vita toolchain.
- SFO/package metadata was generated successfully.
- VPK packaging completed without error.
- artifact contains the expected VitaRetro executable/assets and the intended RetroArch integration files.
- filename remains `VitaRetro_0.1_DEV.vpk`.
- visible app version remains `0.1 DEV`.

If possible, also add a package-content verification step in CI that unpacks/lists the VPK and fails the workflow if required files are absent.

## Existing files to inspect first
- `CMakeLists.txt`
- `src/main.cpp`
- `src/retroarch_manager.hpp`
- `src/retroarch_manager.cpp`
- `scripts/prepare_retroarch.sh`
- `scripts/build_vpk.sh`
- `.github/workflows/` if present
- `README.md`
- `CODEX_HANDOFF.md`

Do not trust prior claims about functionality without inspecting the files and testing the workflow.

## Final deliverable to the user
Return the REAL `VitaRetro_0.1_DEV.vpk` (or its direct GitHub Actions artifact/download location) and a concise report stating what was actually compiled and verified. If it cannot yet be produced, continue debugging rather than substituting a source ZIP or fake VPK.
