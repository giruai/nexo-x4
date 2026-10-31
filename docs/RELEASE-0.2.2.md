# Nexo X4 0.2.2

Thanks to everyone who shared feedback and reproduction details, and to the XCloud4 author for catching and clarifying the XCloud4 Lab identity collision. This release addresses issues #1, #2 and #3.

## Important migration notice

Nexo now uses **NEXO00001** and `/data/nexo-x4/`. It installs separately from older Nexo versions and XCloud4 Lab, which shared `XCLD00001`. XCloud4's public release uses `XCLD00003`.

**Keep the previous installation and its data until the new app is verified. Do not uninstall it to migrate.** Before signing in, open Options → Settings → **Import saved Nexo session**. Import is explicit, checks the current console user, copies the saved renewal credential entirely on the PS4, preserves the original and refuses to overwrite an existing new session. Microsoft expiry/revocation can still require a new sign-in.

## Changes

- **Server regions:** choose Auto or a region offered by Microsoft in Settings. The choice persists and applies to catalog refresh and new game sessions. An unavailable choice falls back visibly to the service default. Only offered regions are accepted; availability and latency are not guaranteed.
- **Controls:** Options now reaches the game. Touchpad click remains View; swipe up opens Xbox Guide; swipe down opens Nexo's menu. L3+R3 is the menu alternative. Local menu and keyboard input remain captured through neutral release.
- **Catalog:** changing region preserves verified public names, artwork and curated rows while applying current title IDs and access flags.
- **Storage:** auth, UI preferences, cache and diagnostics use Nexo's independent namespace. Credits, licenses and upstream attribution are retained.

## Validation and limits

The exact attached PKG was installed and tested on PS4. Tests covered saved-session migration/renewal/restart, region selection, native 720p/1080p, audio PCM, game controls and actual Fortnite search text entry. The complete portable sanitizer suite and native OpenOrbis build passed.

1080p is functional but remains experimental: a complete 60-second animated Battle Royale spectator window measured about **58.8 fresh FPS**, with some queue-full packet drops and frame discards. **Neither profile guarantees constant 60 FPS.** See [detailed validation](VALIDATION.md) and the [build record](releases/0.2.2-build.json) for scope and limitations. Remote Play footage has its own resolution and cadence.

The dependency source archive keeps its original v0.1.0 name because those patched sources are unchanged. Application sources, matching dependency sources and SHA256SUMS accompany the PKG. No accounts, tokens, console configuration, raw logs or private captures are included.

Thank you to [XCloud4](https://github.com/asp11edgr/XCloud4), [PSBox](https://github.com/RafaelNGP/xbox-cloud-gaming-ps5), [OpenOrbis](https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain) and all credited contributors. See [Credits](../CREDITS.md) and [Third-party notices](../THIRD_PARTY_NOTICES.md).
