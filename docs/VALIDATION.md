# Validation and known limits — 0.1.5

This page describes the 0.1.5 beta candidate (`APP_VER` 01.37). It distinguishes the installed build from older releases and portable tests. No logs, account data, console configuration or personal captures are included.

## Build and package checks

Portable C/C++ tests passed with AddressSanitizer and UndefinedBehaviorSanitizer on Linux x86-64. Coverage includes media ingress and NACK state, adaptive-rate policy, video scaling, profile serialization, preferences, overlay copying, queue presentation, status translations, public metadata locale URLs, game-text transactions and the local keyboard. OpenOrbis compilation, linking and PKG creation passed. The installed package hash and runtime build identity matched the tested candidate.

The release source retains the tested program, tests, build scripts, assets and dependency locks. Release documentation was revised after hardware testing; documentation-only differences from the original build snapshot do not indicate a different executable. The PKG is the tested package, not an untested rebuild for a documentation update.

## Observed on PS4

| Area | Evidence | Scope |
| --- | --- | --- |
| Stored account | Account restored without a new device-code sign-in; catalog loaded | Does not prove every future refresh/revocation case |
| Catalog and launch | Verified catalog, artwork, public descriptions, curated rows and Fortnite launch | Availability depends on account and service |
| Controller | Physical mapping was confirmed working by the tester; retained in 0.1.5 | One controller; no claim of local multiplayer |
| Game-text input | Cross insertion, case change, deletion, cancellation, reopening and R2 submission produced Fortnite search results | Supported virtual-keyboard transactions; basic selectable alphabet; password requests cancelled |
| Languages | Settings viewed in all six selectable languages; English public catalog text refreshed and Spanish was restored | Tables and formatting tested; every possible runtime error was not deliberately induced |
| 720p profile | A new session requested 1280×720 and received 1280×720 | Request cap 60 FPS and 8 Mbps; actual delivery varies |
| 1080p profile | A separate session requested 1920×1080; decoded output remained 1280×720 | 1080p delivery is unvalidated |
| Performance | Clean five-second windows approached 59–60 newly presented images/s; other periods supplied about 30 | Not a sustained 60-FPS guarantee |
| Local menu | Comparable 30-FPS periods occurred both open and closed without reported packet loss or local drops | Does not establish the cause of all frame-rate changes |
| Diagnostics | Additional numeric diagnostics could be switched on and off and preferences persisted | Basic logs/media trace are separate; see Privacy |

Two decoder recovery attempts occurred during session startup; playback recovered afterward. A complete match was not played in this exact build. This testing does not establish compatibility with every game, console setup or network.

## Interpreting measurements

The received resolution comes from native decoded pictures, not the 1920×1080 interface framebuffer or a Remote Play screenshot. Frame-rate observations count new images; display refreshes and repeated images are different quantities. Remote Play footage has its own lower resolution and capture rate and cannot establish native stream quality.

A profile request, a successful local control-channel send or an RTCP request is not a server acknowledgement. No conclusion was established about whether account tier, region, device identification or another service policy caused the observed 720p delivery. The project does not promise that changing a setting or buying a subscription will unlock 1080p.

The text-input fix accepts bounded binary and text JSON and treats transaction IDs as opaque, safely escaped identifiers. Both changes were applied together. The successful captured requests were text frames; an isolated binary-framing root cause for the older failure was not proven.

## Release classification

Treat 0.1.5 as a **720p beta candidate**. Experimental 1080p remains a request option with received resolution shown explicitly. Unsupported upstream features are hidden from the PS4 UI. Existing limitations should remain visible in release notes rather than being represented as completed features.
