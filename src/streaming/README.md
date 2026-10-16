# Streaming transport

The native WebRTC adapter connects the Xbox session to libdatachannel and its ICE, DTLS/SRTP, and SCTP dependencies. It sets up receive-only media tracks, passes decrypted RTP to `src/media`, and handles receiver feedback and control messages. The PS4 port and receiver instrumentation are included in the complete patched dependency source bundle described in [Building](../../docs/BUILDING.md).

`adaptive_bitrate.h` evaluates bounded windows of video packet loss, damaged frames, and local backlog. After warm-up it requests within 3–8 Mbps, reduces the desired rate by 20% for qualifying loss, and raises it by 250 kbps after 20 seconds of clean reception. A local backlog does not count as network loss. Observed receive rate is diagnostic; a queued bitrate request does not by itself establish provider acceptance.

The v0.1.0 release enables receiver NACK with zero grace, while diagnostic mailbox control and rumble are disabled. See [Architecture](../../docs/ARCHITECTURE.md) for thread and media ownership.
