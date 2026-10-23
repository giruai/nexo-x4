# Validation and known limits — 0.1.10

This page covers the exact tested 0.1.10 package (`APP_VER` 01.42), with retained earlier evidence identified separately. The [performance report](PERFORMANCE-0.1.10.md) provides measurements and the [build record](releases/0.1.10-build.json) identifies original frozen inputs and the tested package hash. Release documentation was updated after testing; non-documentation inputs and the distributed executable remain unchanged.

## Current package observations

| Area | Observation | Limit |
| --- | --- | --- |
| 1080p | Native SPS coded 1920×1088 / visible 1920×1080 | Not inferred from a screenshot or display resolution |
| Copy stage | Median3.275→2.052 ms against0.1.9; zero extra WB copies or CPU fallback jobs in selected1080 windows | Stage-specific result; different cloud scenes prevent a causal FPS claim |
| Gameplay | 41 five-second windows: median 59.96 freshFPS, minimum 47.65; continuous 130 s at 59.38–60.19 | No full match or universal60 FPS guarantee; queue drops occurred elsewhere |
| 720p | Bounded decoder restart recovered level 3.2→4.2 within unchanged 1280×720 | 80 s animated-menu sample; not a 720p gameplay performance certification |
| Account | Saved sign-in restored after in-place update and again after rollback from a later discarded experiment | Does not cover all future expiry/revocation cases |
| Controls | Remote movement and camera responded; R2 sent without an isolated hit-damage assertion | Physical mapping confirmed earlier and retained; one controller |
| Audio | Nonzero48 kHz PCM observed | No new physical listening, formal underrun or synchronization test |
| Cleanup | Sessions closed successfully, input error0, service cleanup successful; normal UI presentation restored | Limited game/network/hardware coverage |

Portable checks, ASan/UBSan fixtures, async ThreadSanitizer and native OpenOrbis build passed. Tests exercise ownership, fallback and uncertain-completion retention; host fixtures do not emulate PS4 GPU/cache behavior.

## Retained evidence

Earlier Nexo tests verified Fortnite text entry through actual search results and physical controller mapping. Those paths were not changed by 0.1.10. Six-language table/format checks, stored preferences and keyboard/mapping fixtures passed. See the [0.1.9 validation at its immutable tag](https://github.com/giruai/nexo-x4/blob/v0.1.9/docs/VALIDATION.md) for the earlier package's scope. The later [decoder scheduling experiment](experiments/DECODE-SCHEDULING.md) was discarded; its code is absent from this release.

## Remaining limits

Native 1080p delivery is verified, but the tested settings still label it experimental. HSYNC presentation may tear. Native Decode can exceed the 60 FPS budget in demanding scenes. Received resolution, source cadence, newly presented frames and display refresh are different measurements; Remote Play dimensions do not prove native quality.

A successful bitrate request is not a server acknowledgement. Resource-wait estimates are approximate. Account access, region and service policy can change delivery; purchasing a subscription is not promised to unlock a resolution. Local multiplayer, voice chat, rumble, HDR,4K and keyboard/mouse gameplay are not offered. See [Privacy](PRIVACY.md) and [Legal](LEGAL.md).
