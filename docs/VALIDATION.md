# Validation and known limits — 0.1.9

This page describes the stable Nexo X4 0.1.9 release (`APP_VER` 01.41). It separates observations of the exact installed package from retained earlier tests and portable checks. It contains no account data, console configuration, private logs or personal captures.

## Build and correspondence

The release PKG was installed over the existing application; its transferred and installed SHA-256 and runtime build identity matched the tested binary. Portable C/C++ checks passed with AddressSanitizer and UndefinedBehaviorSanitizer on Linux x86-64. The asynchronous conversion fixture also passed ThreadSanitizer. OpenOrbis compilation, linking and packaging passed.

Application code, tests, assets, build scripts and dependency locks match the frozen build inputs. Release documentation was updated after hardware testing, and a public build record was added. These documentation-only changes do not change the executable: the distributed PKG is the tested binary, not an untested rebuild. The [build record](releases/0.1.9-build.json) records the original input hashes, tested package hash and dependency bundle hash. Corresponding application and patched dependency sources accompany the release.

## Observed on PS4

| Area | Evidence | Scope |
| --- | --- | --- |
| Stored account | Saved account restored and refreshed without another device-code sign-in after in-place updates; catalog loaded | Does not prove every future expiry or revocation case |
| 720p recovery | Main H.264 changed from level 3.2 to 4.2 within unchanged 1280×720 geometry; bounded decoder restart succeeded and playback continued | 90 seconds of selected discovery/lobby windows had no local drops or recorded loss; source cadence varied |
| 1080p delivery | Native SPS and decoded geometry confirmed coded 1920×1088 storage, visible 1920×1080 | Native evidence; not inferred from UI output or Remote Play footage |
| 1080p gameplay | 52 five-second windows, about 260 seconds: median 59.18 fresh FPS; a continuous minute near 60 with an empty ingress queue and no recorded loss/drops | Demanding periods fell to about 43–50 FPS; no whole-session 60-FPS guarantee |
| Local menu | Opened and closed during live gameplay; ground/menu observations reached about 59–60 fresh FPS | Does not guarantee the same cadence in every scene |
| Audio | AudioOut opened with 512-sample blocks, Opus decoded, and nonzero console output PCM was observed | No physical speaker listening or formal dropout/synchronization test |
| Controller | Remote virtual movement, camera, jump and R2 exercised in the current package; physical mapping was confirmed in earlier tests and retained | One controller; no local multiplayer claim |
| Game text | Earlier Nexo tests inserted, changed case, erased and submitted text to actual Fortnite search results; protocol and keyboard tests remain | Not physically retyped during the 0.1.9 performance run; basic Latin keyboard; password requests cancelled |
| Languages | Six-language interface and status/keyboard localization retained, with portable table/formatting checks | Every possible runtime error was not deliberately induced |
| Diagnostics | Numeric diagnostics and statistics toggled; disabled again after testing | Basic logs/media trace remain separate; see [Privacy](PRIVACY.md) |
| Cleanup | 720p and 1080p sessions closed cleanly and returned to the catalog | No complete Fortnite match was played in this exact package |

## Performance interpretation

The 1080p route copies each validated native output into independently owned WB storage and converts it on another CPU worker. Native decoding, native output ownership and teardown remain on their original thread; depth remains one. The conversion queue did not drop jobs in the measured run. Native decode plus GPU readback/copy remains a throughput limit in demanding scenes, and local ingress drops still occurred in some periods.

The full 1080p run included discovery movies, lobby/loading and gameplay: 96 active five-second windows had a fresh-FPS median of 55.78. The gameplay-specific median above uses the first observed Battle Royale bus frame as its starting boundary. Brief rates above 60 can reflect backlog catch-up and measurement boundaries; they do not prove a service rate above 60. The comparison with older development builds is indicative, not a controlled matched-scene benchmark.

Received resolution, fresh-image rate, display refresh and repeated images are different measurements. Remote Play has its own capture resolution/rate and cannot establish native stream quality. A lower frame rate alone is not proof of poor connectivity; local queue saturation and native processing time must be considered separately from network loss.

## Remaining limits

1080p is supported and verified in this release. The tested package retains its earlier “experimental” setting label; that label does not mean delivery is limited to 720p. Its HSYNC presentation path can tear. Neither video profile guarantees a particular service response, bitrate or sustained frame rate. Device identity, account access, region and service policy can change delivery.

A local rate request or successful transport send is not a server acknowledgement. Resource-wait estimates are approximate. The project does not promise that a subscription purchase unlocks a resolution. Local multiplayer, voice chat, rumble, HDR, 4K and keyboard/mouse gameplay are not offered. Broader game, network and hardware compatibility has not been established by this limited test.
