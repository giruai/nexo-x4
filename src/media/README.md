# Live media reception

`live_media.c` accepts borrowed, decrypted RTP from the WebRTC transport and copies it into bounded queues. Video packets are reordered and depacketized into complete H.264 access units for the main-thread decoder. Audio packets go to the Opus/AudioOut worker.

Each track validates payload type and SSRC. Loss and malformed access units can trigger an IDR request; a late or foreign packet does not automatically discard a valid in-progress frame. Parameter sets are committed only from complete valid access units. The producer must stop and join before the receiving context is released.

Receiver feedback and adaptive bitrate are described in [Architecture](../../docs/ARCHITECTURE.md).
