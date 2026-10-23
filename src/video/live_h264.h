/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "../media/live_trace.h"
enum { X4_LIVE_WIDTH = 1920, X4_LIVE_HEIGHT = 1080, X4_LIVE_AU_MAX = 2 * 1024 * 1024,
    X4_LIVE_COPY_READERS = 4 };
/* Sink callbacks run only on the CPU converter. A lease owns one RGB buffer
 * through publish; neither callback may read the decoder-owner X4LiveVideo. */
typedef struct {
    void *context;
    uint32_t *(*acquire)(void *context, size_t pixels);
    void (*publish)(void *context, unsigned width, unsigned height,
        uint64_t output, uint64_t completed_us);
} X4LiveVideoSink;
typedef struct {
    void *state;
    X4LiveVideoSink sink; /* Optional; configured before start, survives restart. */
    int stream_profile; /* Immutable decoder session limit; 0=720p, 1=1080p. */
    uint32_t *pixels;
    unsigned width, height;
    uint64_t frames;
    /* Video-owner cumulative timings in sceKernelGetProcessTime microseconds. */
    uint64_t decode_calls, decoded_frames, no_picture_calls, decode_us, decode_max_us;
    uint64_t convert_us, convert_max_us;
    uint64_t conversion_submitted, conversion_dropped; /* Async WB jobs, includes close abandonment. */
    /* Selected-copy wall time includes dispatch/completion, but excludes the
     * first-copy serial reference/compare. Reader times may overlap. */
    uint64_t copy_calls, copy_us, copy_max_us, copy_bytes;
    /* Owner-only GPU transfer attempts/time (includes safe rejection); bytes
     * count completed transfers. Direct bytes are avoided WB->WB memcpy bytes,
     * not an estimated timing saving. First exact checks are excluded. */
    uint64_t gpu_transfer_calls, gpu_transfer_bytes, gpu_transfer_us, gpu_transfer_max_us;
    uint64_t direct_jobs, direct_bytes, direct_fallback_jobs;
    uint64_t gpu_check_calls, gpu_check_bytes, gpu_check_us, gpu_check_mismatch;
    uint64_t wb_copy_calls, wb_copy_bytes, wb_copy_us;
    /* helper_us is the SUM of overlapping elapsed spans across three helpers;
     * it is neither CPU time nor an additional contribution to copy wall. */
    uint64_t copy_parallel_calls, copy_serial_calls, copy_owner_us, copy_helper_us;
    uint64_t copy_wait_us, copy_wait_max_us, forced_preserve_calls;
    uint64_t copy_check_attempts, copy_check_pass, copy_check_mismatch, copy_check_not_checked;
    uint64_t copy_check_bytes, copy_check_us;
    uint64_t last_picture_time_us, picture_gap_max_us;
    /* Initial oversize SPS admission only; native decoder has not started. */
    unsigned startup_waiting_size;
    int error;
    char stage[48];
    /* Session diagnostic ordinals survive decoder restart. Native output has
     * no returned PTS: output IDs never identify an input AU. */
    X4Trace *trace;
    uint64_t trace_au, trace_decode_next, trace_output_next, trace_copy_next;
    uint64_t trace_decoder_epoch, trace_converted_output;
} X4LiveVideo;
/* All public calls run on one video-owner thread. The optional CPU converter
 * touches immutable WB jobs and sink callbacks only; collect merges its counters.
 * Selected1080 async jobs lease independent GPU WB destinations through ACTIVE
 * release. Only the owner calls readback APIs; CPU fallback storage is bounded
 * and lazy. A failed GPU completion/release retains native source ownership.
 * Three optional persistent CPU-only copy helpers finish their disjoint spans
 * before a copied job is published, Decode or native
 * teardown. A partial pool is stopped/joined before serial fallback. The caller
 * retains RGB storage until teardown succeeds. Any failed helper join latches
 * an error and retains the entire state, native and staging storage. */
/* Starts the native queue and AU reservations. Native decoder creation waits
 * for the first admitted Annex-B SPS. Contract changes require session restart. */
int x4_live_video_start(X4LiveVideo *, uint32_t *pixels, size_t pixel_capacity);
/* Select an unpublished destination before a batch. No reader may own it. */
int x4_live_video_set_target(X4LiveVideo *, uint32_t *pixels, size_t pixel_capacity);
enum { X4_LIVE_VIDEO_WAIT_SIZE = 1 };
/* WAIT_SIZE transfers no AU/native ownership; caller retains recovery/IDR wait. */
int x4_live_video_feed(X4LiveVideo *, const uint8_t *, size_t, uint64_t pts);
/* Poll on video owner even without RTP. Returns 1 for a throttled PLI request,
 * negative after 10s from first oversized SPS, zero otherwise. No server ACK. */
int x4_live_video_poll_startup(X4LiveVideo *);
/* Copy/convert the last validated native picture of this batch. With a sink,
 * feed enqueues each picture immediately; conversion/publishing runs on the
 * CPU worker. Native references never cross this owner-thread boundary. */
int x4_live_video_convert_pending(X4LiveVideo *);
int x4_live_video_stop(X4LiveVideo *);

/* Decoder owner only: merge converter counters; reset only interval maxima. */
void x4_live_video_collect(X4LiveVideo *, int reset_maxima);
