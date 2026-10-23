# Nexo X4 0.1.10

Stable PS4 release, `APP_VER` 01.42. Install over the existing application to retain local preferences and saved sign-in.

## Changes

- Remove the extra WB-to-WB image copy in the 1080p asynchronous conversion path. Conversion jobs lease completed, independently owned GPU readback buffers directly.
- Retain initial real-transfer byte checks, bounded CPU fallback and safe retention after uncertain GPU completion or worker shutdown. Native decoder ownership and depth one are unchanged.
- Add separate numeric GPU-transfer, direct-job, fallback and copy-check measurements and regression fixtures.
- Preserve 720p recovery, adaptive bitrate/audio, catalog UI, six languages, saved sign-in, one-controller mapping and the game-text keyboard.

## Observed on PS4

The exact release PKG was installed and tested. Native dimensions confirmed visible 1920×1080 with coded 1088-line storage. The measured steady image-copy stage decreased from 3.275 to 2.052 ms (37.35%); this is a stage-specific improvement, not a 37% FPS claim. Sequential cloud sessions used different scenes and conditions.

The 205-second gameplay sample had a median of 59.96 freshFPS and a minimum of 47.65 FPS. A continuous 130-second interval spanning flight, Xbox overlay and ground measured59.38–60.19 freshFPS without reported packet loss or local drops. Slower periods and local queue drops still occurred elsewhere. A complete Fortnite match was not played in this exact package.

720p survived the observed H.264 level 3.2→4.2 transition within1280×720 and continued playing. Saved sign-in restored without another device code. Camera/movement responded, audioPCM was nonzero, and both sessions closed cleanly. Existing keyboard/mapping tests were retained; physical text entry and speaker listening were not repeated in this optimization run.

Portable checks and sanitizer fixtures passed, including async ThreadSanitizer; native OpenOrbis compilation/link/package passed. Sources and build records identify the tested package; documentation-only updates after testing do not change its executable.

## Limits and downloads

60 FPS is not guaranteed throughout a session. Decode remains a bottleneck in demanding scenes. The existing experimental 1080p setting label and HSYNC path remain; tearing is possible. This adds no 4K, HDR, local multiplayer, voice chat or rumble. The later local 0.1.11 scheduling experiment was rejected and is not included.

Download `Nexo-X4-v0.1.10-ps4.pkg` and verify `Nexo-X4-v0.1.10-SHA256SUMS.txt`. Matching application sources and the unchanged patched dependency source bundle accompany the release. The dependency bundle retains its original v0.1.0 name and locked hash. A legitimate account and applicable game/service access remain required.

See [Validation](VALIDATION.md), [Performance](PERFORMANCE-0.1.10.md), [Building](BUILDING.md), [Credits](../CREDITS.md) and [legal/service information](LEGAL.md).
