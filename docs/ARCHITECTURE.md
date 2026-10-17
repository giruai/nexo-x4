# Architecture

Nexo X4 runs a PS4-native application with a C23 core and media path, a C++17 WebRTC integration, and an adapted PSBox C++ interface. OpenOrbis provides the target SDK and package tools. The native main/UI thread owns controller polling, UI state, video decode, and frame presentation; bounded workers handle network, audio, and public catalog enrichment.

```text
DualShock 4 ──> main/UI ──> PSBox view and stream controls
                    │
Microsoft sign-in ─> auth/session worker ─> Xbox catalog and session
                                             │
                                        WebRTC transport
                                             │ decrypted RTP
                                    bounded media ingress
                                      /             \
                               H.264 video       Opus audio
                                  │                  │
                         Videodec2 → readback    AudioOut worker
                                  │
                         cached NV12 → SSE2 RGBA
                                  │
                         PSBox overlay → VideoOut
```

## Boundaries and ownership

`src/auth` performs Microsoft device-code authorization, token renewal, catalog and Xbox session requests through bounded native HTTPS. It hands the UI public, copied snapshots; credentials and response bodies stay in private worker state. The public OAuth client identifier used for service compatibility is not a secret or a project-owned registration.

`src/streaming` adapts libdatachannel and its dependencies to OpenOrbis. It manages offer/answer, ICE, DTLS/SRTP, media tracks, receiver NACK, and control traffic. `src/media` accepts borrowed decrypted RTP, validates and copies it into bounded queues, reorders H.264 packets into complete access units, and forwards Opus packets to the audio path. Producers must stop and join before their receiving contexts are freed.

`src/video` owns native Videodec2 state and validated NV12 output. A bounded GPU DMA readback transfers write-combining decoder output to CPU-cached memory; the first real transfer is compared byte-for-byte with the established CPU copy before the fast path is enabled. The color and scale path uses SSE2 with scalar handling for remaining pixels. It supports per-session 1280 × 720 or experimental 1920 × 1080 live images, with profile-sized RGB/native allocations, output validation and a direct SSE2 copy for 1080p; UI composition and VideoOut presentation are separate steps. Decoder buffers and any in-flight GPU resources remain owned until completion or safe teardown.

`src/audio` decodes Opus and submits PCM through native AudioOut. `src/input` reads DualShock 4 state and encodes cloud gamepad reports, while the UI consumes menu input locally. The initial home/catalog and in-stream menu are adapted from PSBox in `src/ui/psbox/vendor`; `src/ui/psbox` bridges native snapshots and actions. `src/ui/psbox_backend` keeps a persistent worker for public catalog metadata and images. Metadata may enrich only product IDs already present in the verified service catalog; it cannot create an entitlement or change a launch title ID.

## Rate control and presentation

Adaptive rate control observes valid/lost video packets, damage and local backlog over bounded windows. After warm-up it requests 3–8 Mbps for 720p or 5–15 Mbps for experimental 1080p, cuts the desired rate by 20% on qualifying loss, and adds 250 kbps after 20 seconds of clean reception. A local queue backlog does not count as network loss for this decision. The observed receive rate is diagnostic, not a capacity estimate; a queued rate request does not prove that the service accepted it.

Observed 720p sessions approached 60 frames per second during normal play with compact statistics, while the previous uncached, translucent large menu rendered around 30 frames per second. The new local overlay caches opaque layers and copies only their rectangles directly into VideoOut, avoiding WC destination reads and a full-frame scratch pass; its console frame rate remains to be measured. These results are not frame-rate guarantees; 1080p cloud streaming is unvalidated. User-selectable numeric input/render diagnostics default off; profiles and diagnostics persist in account-free preferences. Profile changes take effect at the next session. This build keeps diagnostic mailbox control disabled, receiver NACK enabled with zero grace, and rumble disabled.

For build inputs and matching source distribution, see [Building](BUILDING.md) and [Third-party notices](../THIRD_PARTY_NOTICES.md).
