# Nexo X4

<p align="center"><img src="assets/branding/icon0.png" width="160" alt="Nexo X4 icon"></p>

![Platform: PS4](https://img.shields.io/badge/platform-PS4-003791) ![Toolchain: OpenOrbis](https://img.shields.io/badge/toolchain-OpenOrbis-334155) ![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C) ![C23](https://img.shields.io/badge/C-23-A8B9CC) ![License: GPL-3.0-only](https://img.shields.io/badge/license-GPL--3.0--only-3DA639) ![Release: v0.1.0](https://img.shields.io/badge/release-v0.1.0-2563EB)

Nexo X4 is an independent Xbox Cloud Gaming client for PS4 homebrew, built with the OpenOrbis toolchain. It combines the XCloud4 native streaming work with a PS4 adaptation of RafaelNGP's PSBox interface. The project is unaffiliated with Microsoft, Xbox, Sony, or PlayStation.

The first release is **v0.1.0** (`APP_VER` `01.32`). It provides Microsoft device-code sign-in, a catalog and game launch flow, DualShock 4 input, and native video and audio playback. In observed 720p sessions, normal play with compact statistics approached 60 frames per second; opening the large menu brought rendering to around 30 frames per second. These are observations, not promised frame rates. 1080p cloud streaming has not been validated.

The receiver uses adaptive bitrate requests from 3 to 8 Mbps: qualifying packet loss reduces the requested rate by 20%, followed by 250 kbps increases after 20 seconds of clean reception. Video output uses GPU DMA readback from write-combining memory to a CPU-cached buffer, then SSE2 color conversion and scaling. Public catalog metadata and images are fetched by a persistent refresh worker. Actual frame rate and network quality depend on the stream, console, and connection.

[Download the latest release](https://github.com/giruai/nexo-x4/releases/latest) · [Build guide](docs/BUILDING.md) · [Credits](CREDITS.md)

## Build and install

Build from source on Linux x86-64 with OpenOrbis and the complete patched dependency source archive for this release. See [Building](docs/BUILDING.md) for prerequisites, the toolchain and archive inputs, host checks, and package creation. The expected package is `dist/Nexo-X4-v0.1.0-ps4.pkg`.

Install the resulting package through your usual PS4 homebrew package workflow. A Microsoft account and access to Xbox Cloud Gaming in a supported region are required for the service. Sign-in uses a public OAuth client identifier for compatibility; it is **not** a secret or a Nexo X4-owned app registration. Service availability and authorization remain controlled by Microsoft.

The package retains title ID `XCLD00001` for migration compatibility. That ID is not the product name. This release ships with diagnostic mailbox control off, receiver NACK on, NACK grace at 0 ms, and rumble off.

## Project guide

- [Architecture](docs/ARCHITECTURE.md) describes the application boundaries and media path.
- [Building](docs/BUILDING.md) describes reproducible local build inputs and checks.
- [Credits](CREDITS.md) distinguishes incorporated work from research references.
- [Third-party notices](THIRD_PARTY_NOTICES.md) and [retained license texts](docs/licenses/README.md) describe redistributed components and corresponding source.
- [Brand artwork](assets/branding/README.md) contains the editable icon source and its attribution.

Nexo X4 application code is licensed under [GPL-3.0-only](LICENSE). Incorporated third-party components retain their own notices and licenses. A distributed package must be accompanied by its matching application source and complete patched dependency source bundle; a list of upstream URLs alone does not describe the distributed dependency modifications.
