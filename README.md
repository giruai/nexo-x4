# Nexo X4

<p align="center"><img src="assets/branding/icon0.png" width="160" alt="Nexo X4"></p>

[![Host checks](https://github.com/giruai/nexo-x4/actions/workflows/host-checks.yml/badge.svg)](https://github.com/giruai/nexo-x4/actions/workflows/host-checks.yml)
![PS4 homebrew](https://img.shields.io/badge/platform-PS4-003791)
![OpenOrbis](https://img.shields.io/badge/toolchain-OpenOrbis-334155)
[![Release 0.1.9](https://img.shields.io/badge/release-0.1.9-2563EB)](https://github.com/giruai/nexo-x4/releases/tag/v0.1.9)
![GPL-3.0-only](https://img.shields.io/badge/license-GPL--3.0--only-3DA639)

**An independent Xbox Cloud Gaming client for PS4 homebrew.** Nexo X4 combines XCloud4's native streaming foundation with a PS4 adaptation of RafaelNGP's PSBox catalog interface. It is unofficial and unaffiliated with Microsoft, Xbox, Sony or PlayStation.

**Current stable release: 0.1.9.** The repository also contains the [0.1.10 performance candidate](docs/PERFORMANCE-0.1.10.md), built and tested on PS4 but not yet released. Both 720p and actual 1920×1080 streaming have been verified on PS4. The 1080p Fortnite gameplay sample had a median of about 59 newly presented frames/s, including a continuous minute near 60. More demanding scenes fell to about 43–50 FPS; 60 FPS is not guaranteed throughout a session. See [validation and known limits](docs/VALIDATION.md).

[Releases and downloads](https://github.com/giruai/nexo-x4/releases) · [Build guide](docs/BUILDING.md) · [Changelog](CHANGELOG.md) · [Credits](CREDITS.md)

## What works

- Microsoft device-code sign-in and locally stored sign-in renewal, so a valid saved account can be restored after restarting the application.
- A browsable cloud catalog with public artwork, descriptions, curated rows and game launch.
- One DualShock 4 controller, native H.264 video and Opus audio playback.
- A local keyboard for supported Xbox game-text requests. Fortnite search was tested through to its results.
- English, Brazilian Portuguese, Spanish, French, German and Italian interface text, including client statuses, queue details and keyboard controls. Public catalog text is requested in the selected locale where Xbox provides it.
- Saved video-profile and additional-diagnostics options, a local in-game menu, compact statistics and optional resource-wait estimates supplied by Xbox.

A legitimate Microsoft account, a supported service region and the selected game's required access are necessary. Fortnite may be available without a paid Game Pass subscription where Microsoft offers it. The service decides access; this client cannot grant an entitlement or guarantee continued compatibility.

## Video profiles and limits

| Setting | What it requests | What was verified |
| --- | --- | --- |
| 720p | Up to 60 FPS; adaptive requests within 3–8 Mbps | Decoded 1280×720, including clean measured windows near 60 new images/s |
| 1080p | Up to 60 FPS; adaptive requests within 5–8 Mbps | Native 1920×1080 verified; about 59 FPS median in the sampled Fortnite gameplay |

Profiles apply to the **next cloud session**. The in-game menu shows the received dimensions and measured frame rate separately from the requested profile. Neither profile guarantees a particular service response or sustained frame rate. Catalog movies and other source-limited scenes may supply fewer frames; demanding gameplay can also exceed the client's processing budget.

Adaptive bitrate lowers the requested rate after qualifying packet loss and raises it gradually after clean reception. A successful local request is not confirmation that Xbox accepted it, and received bitrate is not a connection-speed measurement. Resource-wait estimates are approximate service estimates, not a personal queue position or guaranteed countdown.

Local multiplayer, microphone/voice chat, keyboard/mouse gameplay, rumble, HDR, 4K and remote-console discovery are not offered by this PS4 port. Do not infer support from features in the upstream PS5 project. The game-text keyboard offers Latin letters, digits and common punctuation; it preserves valid UTF-8 default text and cancels password-scope requests.

## Install and use

1. Obtain a release PKG and verify its SHA-256 against the attached checksum file. Check the version and known limits in the release notes.
2. Install it through the normal package workflow on a PS4 that already supports homebrew execution. To update Nexo X4, install over the existing application rather than uninstalling it.
3. Complete Microsoft sign-in using the displayed official verification address and code. Open the catalog and select a title available to your account.
4. Use Options for language, next-session video profile and additional diagnostics. During streaming, Options opens Nexo's local menu; its Xbox button action opens the service's separate Guide.

The latest stable package version is **0.1.9**, its PS4 `APP_VER` is **01.41**, and the title ID remains `XCLD00001` for update compatibility. Generated output is `dist/Nexo-X4-v0.1.9-ps4.pkg`. Sign-in may still be required again if Microsoft expires or revokes renewal credentials.

The local game-text keyboard uses the D-pad to choose, Cross to insert, Triangle to change case, Square to erase, R1 for space, R2 to submit and Circle to cancel. This is separate from a game's own on-screen keyboard.

## Build from source

Use Linux x86-64, the pinned OpenOrbis toolchain and the complete patched dependency source archive. An amd64 Linux guest can be used on Apple Silicon. Follow [Building](docs/BUILDING.md) for isolated setup, input hashes, host checks, native compilation and packaging. A host test does not emulate PS4 drivers or validate a game session.

The dependency archive retains its original v0.1.0 filename because those dependency sources are unchanged; `dependencies.lock.json` identifies its exact bytes. Each release must include matching application sources, patched dependency sources and retained notices.

## Privacy, service use and attribution

Refresh credentials are stored locally with restricted permissions but are not encrypted; privileged filesystem access can read them. Logging out clears the local credentials, not Microsoft's grants. Additional numeric input/render diagnostics default off; basic lifecycle logs and a local numeric media trace are separate. See [Privacy](docs/PRIVACY.md).

Nexo X4 uses service interfaces without an official integration agreement. Its public OAuth compatibility identifier is not a client secret or a Nexo-owned registration. Software licensing does not confer service access, account protection or platform endorsement. Read [Legal and service use](docs/LEGAL.md).

Application code is [GPL-3.0-only](LICENSE). Incorporated components retain their own licenses and authorship. Thanks especially to [XCloud4](https://github.com/asp11edgr/XCloud4), [PSBox](https://github.com/RafaelNGP/xbox-cloud-gaming-ps5) and [OpenOrbis](https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain). See [Credits](CREDITS.md), [Third-party notices](THIRD_PARTY_NOTICES.md) and the [license inventory](docs/licenses/README.md) for source revisions, incorporated work and technical references. The public runtime supplement records known inputs and the remaining source-provenance limits of SDK prebuilt archives.

## Documentation

- [Validation and known limits](docs/VALIDATION.md)
- [Build and packaging instructions](docs/BUILDING.md)
- [Architecture](docs/ARCHITECTURE.md)
- [Changes by version](CHANGELOG.md)
- [Editable brand artwork](assets/branding/README.md)
