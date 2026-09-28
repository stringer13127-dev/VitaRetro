# VitaRetro — handoff after 0.1 DEV shell

## Fixed product rules
- Open source; public GitHub repository.
- Zero-euro operating budget.
- Up to five user-configurable sources initially.
- Each source is defined by ONE global site URL, not one URL per game.
- Vita displays a QR; phone opens a small pairing page where the user pastes the global URL.
- Tabs inside each source: Jeux / Emulateurs / Homebrews.
- Global-search toggle searches all enabled sources.
- Global results remain grouped under each source (StreamFlix behavior); do not merge duplicates by default.
- RetroArch is the default transparent execution backend when appropriate. User should click a game and launch it without manually selecting a core.
- Advanced users may override emulator/core choices.
- Only emulators/backends that can actually run on PS Vita should be surfaced.
- UI screenshots must reflect the actual implemented UI; no marketing mockups.

## 0.1 DEV implemented shell
- Native 960x544 framebuffer renderer, no external graphics dependency.
- Five source tiles and controller navigation.
- Tabs.
- Global-search toggle.
- Demo grouped-results screen.
- Source detail placeholder for QR/global URL provider.
- GitHub Actions build workflow using official vitasdk/vitasdk:2026.08.

## Next engineering tasks
1. Compile and install 0.1 DEV on real Vita; fix any SDK/link/runtime issue before increasing version.
2. Add Vita IME/search text entry.
3. Implement provider interface: probe, catalog search, categories, detail page, emulator list, homebrew list.
4. Implement safe HTTP(S) client and cache with per-source timeout/failure isolation.
5. Implement QR-pair flow through zero-cost Cloudflare Worker/Pages endpoint.
6. Add source enable/disable and source health status.
7. Add metadata normalization while preserving per-source result sections.
8. Detect/install supported emulators from authorized public releases.
9. Create launch rules: platform -> backend -> core/executable -> content path.
10. Launch RetroArch content directly without exposing core selection in normal mode.
11. Add artwork cache and graceful missing-image placeholders.
12. Add update system for VitaRetro and provider definitions.

## Version discipline
Keep 0.1 DEV until the user confirms that the VPK installs and reaches the main menu on real hardware.
