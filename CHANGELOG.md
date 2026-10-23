# Changelog

## 0.1.10 — unpublished performance build · PS4 APP_VER 01.42

- Lease cached GPU readback destinations directly to bounded 1080p conversion jobs, eliminating the extra WB-to-WB memcpy after DMA.
- Preserve per-context real-transfer checks, bounded CPU fallback, native ownership and retention after uncertain GPU/worker teardown.
- Add separate GPU transfer, direct-job, fallback, WB-copy and initial-check measurements and failure-injection regression tests.

Portable checks, sanitizer tests and native build passed. PS4 testing measured a 37% shorter image-copy stage (3.28→2.05 ms), verified actual 1080p and 720p SPS recovery, and retained saved sign-in. A 130-second interval measured 59.4–60.2 fresh FPS; demanding scenes still dropped below 60. See [measurements and limits](docs/PERFORMANCE-0.1.10.md). Stable 0.1.9 remains the public release.

## 0.1.9 — stable release · PS4 APP_VER 01.41

- Correct observed 720p H.264 level 32→42 transitions without increasing the selected resolution limit: native admission uses the tested level 42 ceiling independently of the requested SDP level. DPB, geometry, crop and memory bounds remain enforced.
- Copy each1080p native output into a bounded, independently owned WB staging queue and convert on a separate CPU worker. Keep native Decode/copy/teardown on its owner and decoder depth 1.
- Publish RGB through existing reader/writer leases; replace queued old frames without recycling active buffers. Join the converter before restart/free and retain storage after uncertain teardown.
- Add async job/drop measurements, per-output ownership fixtures and exact720p SPS transition regression tests. Preserve UI, audio, input and saved sign-in.

Portable and native checks passed. Hardware validation confirmed 720p recovery and actual 1080p; gameplay reached a median of about 59 fresh FPS, with demanding scenes around 43–50 FPS. See [stable validation](docs/VALIDATION.md).

## 0.1.8 — local diagnostic build · PS4 APP_VER 01.40

Added rejection-only scalar SPS fields. Hardware identified the720 failure as level 42 arriving within unchanged1280×720 geometry after level 32. Guards were unchanged in this diagnostic build.

## 0.1.7 — SPS recovery candidate · PS4 APP_VER 01.39

- Add recovery for a hardware-observed 720p regression: a valid changed SPS invokes bounded owner-thread decoder teardown/restart and a fresh keyframe request instead of cancelling the cloud session. Invalid SPS and unsafe teardown still fail safely.
- Retain the 0.1.6 negotiation, audio and presentation changes.

0.1.6 demonstrated actual 1920×1080 on PS4, but native presentation was around 30 FPS and its 720p session failed after a parameter change. 0.1.7 hardware testing confirmed actual 1080p but did not sustain 60 FPS;720p still failed native level admission after restart.0.1.8 identified that separate cause.

## 0.1.6 — development candidate · PS4 APP_VER 01.38

- Adapt XCloud4 G8 web/Windows negotiation for experimental 1080p, with video SDP capped at 8 Mbps and adaptive requests within 5–8 Mbps. Keep the native 720p profile.
- Validate H.264 SPS and coded versus visible dimensions before creating the decoder; handle coded 1088-line storage for visible 1080p.
- Use HSYNC presentation only for the experimental 1080p session; keep VSync for 720p and UI. HSYNC may show tearing.
- Add adaptive audio with 512-sample output and a 1024-sample fallback.
- Preserve PSBox UI, six languages, stored sign-in renewal, game text, input mapping, diagnostics and ABR.

Hardware validation is pending. These changes request 1080p and target 60 FPS; neither is guaranteed. Decoder depth remains 1 because input buffer retirement for upstream depth 2 is not independently established.

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
