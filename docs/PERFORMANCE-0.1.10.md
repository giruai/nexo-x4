# Nexo X4 0.1.10 performance build

Performance candidate tested on PS4 on 2026-10-23, PS4 APP_VER 01.42. The latest public stable release remains 0.1.9. This candidate has not been published as a release.

The 1080p async conversion queue can now lease independent GPU-readable/cached WB destinations directly. This removes the extra memcpy from the DMA destination into a heap job. The decoder owner remains the sole caller of native decode, transfer and teardown APIs; the converter reads only a completed, exclusively leased destination. Initial real transfers are compared byte-for-byte against the owner scratch copy. Safe GPU rejection or mismatch uses bounded CPU fallback; uncertain submission/completion/release retains all potentially owned storage before native decoder deletion.

The 720p path and level 3.2→4.2 recovery remain. Decoder depth stays one. This build does not change bitrate policy, service identity, audio, input or the public UI. Added counters distinguish completed GPU transfers, direct jobs/bytes, fallback jobs, retained WB memcpy traffic, and initial exact-check work. Initial checking is excluded from steady transfer/copy timings.

Portable tests exercise per-output content with a converter blocked while 20 later frames recycle native output, newest-frame supersession, padded coded 1088 geometry, GPU unavailable/rejection/mismatch, partial allocation, uncertain completion, release failure, native deletion failure and worker join failure. These tests do not emulate GPU cache coherency or establish FPS on a console.

## Native measurements

The transferred and installed package hashes and runtime identity matched the tested candidate. The [build record](releases/0.1.10-build.json) records the original inputs. Documentation was updated after testing; these documentation-only changes do not change the executable.

| Measurement | 0.1.9 reference | 0.1.10 candidate |
| --- | ---: | ---: |
| Native visible image | 1920×1080 | 1920×1080 |
| Bytes per copied image | 3,133,440 | 3,133,440 |
| Median steady image-copy stage | 3.275 ms | 2.052 ms |
| Selected active sample duration | 445 s | 405 s |
| Median fresh presented FPS | 59.56 | 59.78 |

The measured copy stage was 37.35% shorter, saving about 1.22 ms per image. Selected candidate windows recorded 22,343 direct jobs, zero extra WB copies, zero CPU fallback jobs, and one initial exact check with zero mismatch. Only contexts actually used on hardware were checked; portable fixtures also exercise concurrent slot ownership. The initial check took 76.75 ms once and is excluded from steady timing. Submitted jobs and fresh presented frames are different counters.

These were sequential cloud sessions with different scenes and service conditions. Copy timings support the stage-specific improvement; the small FPS median difference is not a controlled demonstration of an FPS gain. The candidate gameplay sample covered spawn island, flight and coastal ground: 41 five-second windows had a median of 59.96 fresh FPS and a minimum of 47.65. A consecutive 130-second interval spanning flight, the Xbox overlay and ground measured 59.38–60.19 fresh FPS with no reported network loss or local packet drops. The whole 1080p sample still included 179 local packet drops across 17 queue-full windows.

Native Decode remained the largest measured cost: gameplay window means reached 16.46 ms before GPU transfer. This still exceeds a 60-FPS budget once other owner work is added. Neither this test nor a screenshot establishes sustained 60 FPS across all scenes. Existing experimental HSYNC presentation can tear.

## Regression checks and limits

- Actual SPS confirmed coded 1920×1088 / visible 1920×1080, Main profile, level 4.2, native depth one.
- 720p handled the observed level 3.2→4.2 change within unchanged 1280×720 geometry: one bounded decoder restart, then continued animated Fortnite content using the legacy serial path. Its 80-second menu sample had variable source cadence and no reported local packet drops; this was not a 720p gameplay performance certification.
- Saved sign-in restored after the in-place update without another device-code authorization, and the catalog loaded.
- Camera and movement responded. R2 was sent without a separate hit-damage assertion. Earlier mapping/keyboard behavior is retained and portable input tests passed; Fortnite text entry was not repeated during this optimization run.
- Remote Play received nonzero 48 kHz audio PCM; no physical listening or formal synchronization/dropout test was performed.
- Both sessions closed with successful service cleanup and no input error. The 1080p session restored normal UI presentation after close.
- Full portable checks, ASan/UBSan sync and async fixtures, async ThreadSanitizer and native OpenOrbis compilation/link/package passed.

No private logs, account data, console configuration or raw captures are included. Remote Play footage has its own resolution and does not prove native stream quality. The tested 0.1.9 release and its assets remain unchanged.

## Proposed next goal

Profile native Decode and scheduling in fixed 1080p scenes, distinguish decoder cost from queue age, and test one bounded change against this candidate. Accept only a repeatable improvement without extra latency, unsafe native concurrency, broken 720p recovery or regressions in audio, input and saved sign-in. Do not change bitrate policy or decoder depth based on FPS alone.
