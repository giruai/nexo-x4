# Audio

`live_audio.c` consumes queued Opus RTP, decodes it with the linked Opus 1.5.2 library, and submits stereo PCM to native AudioOut. Submitted PCM remains owned by the audio context until consumption or closure completes. The audio worker and its queue stop before their state is released.

`demo_audio.c` is a local synthetic tone path useful for development; it is separate from live cloud audio. Opus's retained license and matching dependency source obligations are listed in [Third-party notices](../../THIRD_PARTY_NOTICES.md).
