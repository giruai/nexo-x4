# Display and video

`display.c` owns VideoOut frame buffers and presentation. `live_h264.c` feeds complete H.264 access units to native Videodec2 and validates its NV12 output. The live decoder is configured for up to 1280 × 720. `videodec2_abi.h` adapts public OpenOrbis declarations with attribution in [Third-party notices](../../THIRD_PARTY_NOTICES.md).

Decoded output begins in write-combining memory. `gpu_readback.c` provides bounded DMA transfer to CPU-cached memory, with a byte-for-byte comparison against the established CPU copy before enabling the fast path. The conversion and scaling path uses SSE2 and handles remaining pixels with scalar code. Resource ownership covers any in-flight GPU transfer and decoder output until safe completion.

The 1920 × 1080 display surface is the UI/output size, not a validated 1080p cloud-stream resolution. Observed 720p sessions approached 60 frames per second with compact statistics; an open large menu rendered around 30 frames per second. These observations are not frame-rate guarantees. `h264_demo.c` remains a local sample path, separate from live playback.
