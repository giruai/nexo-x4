# Nexo X4 0.1.9

Stable release for PS4, `APP_VER` 01.41. Install over the existing application to retain local preferences and saved sign-in.

## Changes

- Actual 1920×1080 cloud video, verified through native decoded dimensions.
- Faster 1080p processing: a separate CPU worker converts copied NV12 frames while the native owner continues decoding. Bounded frame leases protect active buffers during replacement, restart and shutdown.
- Correct 720p H.264 level changes: Xbox can change level 3.2 to 4.2 without changing 1280×720 geometry. Decoder admission now separates native capability from the requested SDP level, retaining resolution, DPB, crop and memory checks.
- Adaptive audio prefers 512-sample output blocks and falls back to 1024 when required.
- Retained PSBox catalog UI, six languages, stored sign-in renewal, game-text keyboard, single-controller mapping, optional diagnostics and adaptive bitrate.

## Verified results

The exact release PKG was installed and tested on PS4. The 720p parameter transition recovered successfully. Actual 1080p Fortnite gameplay had a median of about 59 fresh FPS over the sampled 260 seconds, including a continuous minute near 60 without recorded loss or local drops. Local menu, movement, camera, jump, R2 and audio output were exercised. Saved sign-in restored without another device code.

Demanding scenes still fell to about 43–50 FPS. This is not a promise of stable 60 FPS throughout every game or connection, and a complete Fortnite match was not played in this exact package. The existing settings label still calls 1080p experimental; native 1080p delivery is nevertheless verified. HSYNC presentation can tear. There is no new 4K, HDR, local multiplayer, rumble or voice-chat support.

See [Validation](VALIDATION.md) for test scope and measurements, [Building](BUILDING.md) for compilation, and [Credits](../CREDITS.md) for incorporated upstream work. The tested executable is unchanged by the post-test release documentation update.
