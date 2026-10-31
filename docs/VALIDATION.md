# Validation and known limits — 0.2.2

Release 0.2.2, PS4 APP_VER 02.02, independent title ID `NEXO00001`. The [build record](releases/0.2.2-build.json) identifies the exact tested package. The previous 0.1.10 release and its assets are unchanged. Final documentation updates do not change the executable.

## Native observations

| Area | Observation | Limit |
| --- | --- | --- |
| Installation | New identity installed separately; full installed-package hash matched; the application at the legacy identity remained unchanged | No exhaustive collision registry is claimed |
| Saved session | Explicit import renewed and saved access; closing/reopening restored it without another device-code login; the 0.2.2 update restored it again | The old credential is read only, copied on the console, never exported; Microsoft can expire/revoke renewal credentials |
| Regions | Auto and an explicit service-offered region applied to catalog login; fresh game-session login used the persisted selection | Availability is per current service response; no location/latency promise or custom URL entry |
| Catalog UI | Region round trip retained public names, artwork and curated rows with no metadata replay | Current catalog IDs and access flags are rebuilt; cached artwork does not grant access |
| Controls | Options opened Fortnite's menu; up-swipe opened/closed Xbox Guide; down-swipe and L3+R3 opened Nexo's menu; touchpad click retained View | Tests used paired Remote Play input through the native pad path; not every physical controller/firmware was exercised |
| Keyboard | Actual Fortnite search received `ab`; X typed, Triangle changed case, Square deleted and R2 sent. Cancel preserved the existing query | Native text prompt behavior, not keyboard/mouse gameplay support |
| Audio | Opus/AudioOut operated and Remote Play received nonzero 48 kHz PCM | No new physical listening or formal synchronization/underrun certification |
| Cleanup | Service cleanup and input send results succeeded after profile sessions | Coverage is one game and a limited set of sessions/network conditions |

Touchpad View and pickaxe animation were observed on migration candidate 0.2.1; that pad mapping is unchanged in 0.2.2. Options, gestures, menu fallback and Fortnite keyboard entry were repeated on final 0.2.2.

Both native profiles were exercised in 0.2.2: SPS confirmed 1280×720 and coded 1920×1088 / visible 1920×1080. Resolution was measured from the actual decoder stream, independently of display scanout and Remote Play capture size. Changing profile applies to a new game session and did not require another account login.

## Cadence measurements

The media implementation is retained from 0.1.10. A migration candidate 0.2.1 with this same media/input path measured 59.39 newly drawn/presented FPS over 60.06 s of 720p Battle Royale, with no reported loss, queue-full packets or damaged-frame discards. Final0.2.2 also verified 720p; an animated Discovery/menu sample delivered 36.12 decoded pictures/s and 35.96 new frames/s with no local packet drops. These are different phases; the lower incoming cadence is not evidence of an equivalent decoder capacity limit.

A complete 60.02 s window in final0.2.2 at 1080p, spectating an animated Battle Royale world after a match, measured 58.77 new frames/s. It recorded 6 queue-full packet drops and 76 frame-discard events. Those events and variable native Decode cost prevent a claim of constant 60 FPS. A separate requested motion window lacked its complete log interval and is excluded from the figures.

These sequential cloud sessions are not a controlled performance comparison. The [0.1.10 performance report](PERFORMANCE-0.1.10.md) retains the earlier copy-stage results and decoder limits. Incoming pictures, fresh presentation and screen refresh are different counters. Existing experimental HSYNC presentation may tear. Remote Play footage is functional evidence and has its own resolution/cadence.

## Portable checks and storage scope

The complete Linux x86-64 portable suite, AddressSanitizer/UndefinedBehaviorSanitizer fixtures and native OpenOrbis compile/link/package passed. Fixtures cover service-region validation/selection/fallback, persisted preferences, controller gestures and neutral capture, six-language rendering/text fit, identical/changed catalog refresh, and storage import/renewal/races/failures. Host tests do not emulate the PS4 GPU or native filesystem permissions; a synthetic directory-commit probe also passed on the console before legacy import.

New credentials use private 0700 directories and a 0600 plaintext refresh-token file. Import refuses an existing saved session, preserves the legacy source and validates local user/client/scope. Complete nonempty session directories cannot replace each other by directory rename. POSIX can replace an empty directory created after preflight; this does not isolate a hostile writer with the same effective UID, and file fsync alone does not certify power-loss durability of directory entries. No real credential is included in tests, sources, logs intended for publication, or release assets.

Full physical-controller coverage, arbitrary games/networks, universal 60 FPS, local multiplayer, voice chat, rumble, HDR, 4K and keyboard/mouse gameplay are not claimed. A successful bitrate request is not a server acknowledgement; resource-wait estimates are approximate. See [Privacy](PRIVACY.md) and [Legal](LEGAL.md). Private console configuration, account data, raw logs and captures are excluded from this repository.
