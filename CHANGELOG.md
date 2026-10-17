# Changelog

## 0.1.5 — beta candidate · PS4 APP_VER 01.37

- Add local game-text entry and safe Xbox transaction completion/cancellation; Fortnite search tested through to results.
- Handle binary and text JSON requests and opaque transaction IDs without logging entered text or transaction identifiers.
- Localize client statuses, HTTPS stages, queue details and keyboard controls in six languages, with initial status capitalization.
- Refresh public catalog text in the selected locale while preserving account entitlements and market.
- Show received video dimensions/FPS separately from the requested next-session profile.
- Retain the tested single-controller mapping. Hide decorative four-controller rows and unsupported inherited UI actions.
- Retain saved sign-in renewal, adaptive-rate requests, optional diagnostics and resource-wait information.

Known limits: Xbox delivered 720p when 1080p was requested; 60 FPS is not guaranteed; startup decoder recovery was observed. A complete match was not played in this exact candidate. See [Validation](docs/VALIDATION.md).

## 0.1.4 — local test candidate · APP_VER 01.36

Pruned unsupported UI and added explicit resolution preference, display capability information and the initial local game-text path. The first Fortnite search test did not display the local keyboard; 0.1.5 corrects request handling. This was a local test build, not a separately published release.

## 0.1.3 — earlier draft candidate · APP_VER 01.35

Combined saved video profiles, optional diagnostics, cached menu composition and resource-wait estimates with legal/privacy documentation and retained runtime sources/notices. Superseded for release preparation by 0.1.5.

## 0.1.1–0.1.2 — local candidates

Introduced profile/diagnostic controls and optional Xbox resource-wait estimates. These versions were development candidates rather than published releases.

## 0.1.0 — first release · APP_VER 01.32

Initial Nexo X4 release combining the XCloud4 native PS4 client with the adapted PSBox interface, Microsoft sign-in, catalog/game launch, controller input and native media playback. Original assets remain unchanged.
