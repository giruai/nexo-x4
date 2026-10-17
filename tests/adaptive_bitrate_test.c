/* SPDX-License-Identifier: GPL-3.0-only */
#include "../src/streaming/adaptive_bitrate.h"
#include <stdio.h>
#include <stdint.h>

#define CHECK(c) do { if (!(c)) { \
    fprintf(stderr, "adaptive_bitrate_test:%d: %s\n", __LINE__, #c); return 1; \
} } while (0)

typedef struct { X4AdaptiveBitrate policy; X4AbrSnapshot counters; } Fixture;

static void begin(Fixture *f)
{
    f->counters = (X4AbrSnapshot){0};
    x4_abr_init(&f->policy, 0);
    (void)x4_abr_update(&f->policy, 0, &f->counters);
}

static X4AbrDecision tick(Fixture *f, uint64_t second, uint64_t valid,
    uint64_t lost, uint64_t damaged, uint64_t pli, uint64_t oldest)
{
    f->counters.valid_video_packets += valid;
    f->counters.lost_packets += lost;
    f->counters.damaged_frames += damaged;
    f->counters.pli_requests += pli;
    f->counters.received_video_bytes += valid * 1000;
    f->counters.ingress_oldest_us = oldest;
    return x4_abr_update(&f->policy, second * 1000000u, &f->counters);
}

static int warm(Fixture *f)
{
    begin(f);
    for (uint64_t s = 2; s <= 10; s += 2) {
        X4AbrDecision d = tick(f, s, 100, 0, 0, 0, 0);
        CHECK(d.desired_kbps == 8000);
        if (s < 10) CHECK(!d.send_due);
        else {
            CHECK(d.send_due && d.send_kbps == 8000 && d.sent_kbps == 0);
            x4_abr_note_send(&f->policy, s * 1000000u, d.send_kbps, true);
        }
    }
    return 0;
}

static int test_empty_and_warmup(void)
{
    Fixture f;
    begin(&f);
    for (uint64_t s = 2; s <= 12; s += 2) {
        X4AbrDecision d = tick(&f, s, 0, 0, 0, 0, 0);
        CHECK(d.reason == X4_ABR_EMPTY && d.desired_kbps == 8000);
    }
    for (uint64_t s = 14; s <= 22; s += 2) {
        X4AbrDecision d = tick(&f, s, 100, s == 14 ? 10 : 0, 0, 0, 0);
        CHECK(d.desired_kbps == 8000);
        if (s < 22) CHECK(!d.send_due);
        else {
            CHECK(d.send_due && d.send_kbps == 8000);
            x4_abr_note_send(&f.policy, s * 1000000u, d.send_kbps, true);
        }
    }
    X4AbrDecision d = tick(&f, 24, 90, 10, 0, 0, 0);
    CHECK(d.reason == X4_ABR_LOSS_BURST && d.desired_kbps == 6400 && d.send_due);
    return 0;
}

static int test_false_congestion_and_receive_rate(void)
{
    Fixture f;
    CHECK(warm(&f) == 0);
    X4AbrDecision d = tick(&f, 12, 100, 0, 5, 2, 250000);
    CHECK(d.reason == X4_ABR_LOCAL_BACKLOG && d.desired_kbps == 8000);
    CHECK(d.receive_kbps == 400);
    d = tick(&f, 14, 100, 0, 0, 0, 0);
    CHECK(d.desired_kbps == 8000 && !d.send_due);
    f.counters.local_dropped_packets += 3;
    d = tick(&f, 16, 98, 2, 0, 0, 0);
    CHECK(d.reason == X4_ABR_LOCAL_BACKLOG && d.desired_kbps == 8000);
    d = tick(&f, 18, 100, 0, 5, 2, 0);
    CHECK(d.desired_kbps == 8000 && !d.send_due);
    /* Neither low receive rate, damage/PLI alone, nor local queue age is
     * evidence of the packet loss required for bitrate reduction. */
    return 0;
}

static int test_transient_sustained_and_hold_down(void)
{
    Fixture f;
    CHECK(warm(&f) == 0);
    X4AbrDecision d = tick(&f, 12, 98, 2, 0, 0, 0);
    CHECK(d.desired_kbps == 8000 && !d.send_due);
    d = tick(&f, 14, 100, 0, 0, 0, 0);
    CHECK(d.desired_kbps == 8000);
    d = tick(&f, 16, 98, 2, 0, 0, 0);
    CHECK(d.desired_kbps == 8000 && !d.send_due);
    d = tick(&f, 18, 98, 2, 0, 0, 0);
    CHECK(d.reason == X4_ABR_LOSS_SUSTAINED && d.desired_kbps == 6400);
    CHECK(d.send_due && d.send_kbps == 6400 && d.sent_kbps == 8000);
    x4_abr_note_send(&f.policy, 18000000, 6400, true);
    d = tick(&f, 20, 90, 10, 0, 0, 0);
    CHECK(d.reason == X4_ABR_HOLD_DOWN && d.desired_kbps == 6400);
    for (uint64_t s = 22; s < 28; s += 2) (void)tick(&f, s, 90, 10, 0, 0, 0);
    d = tick(&f, 28, 90, 10, 0, 0, 0);
    CHECK(d.reason == X4_ABR_LOSS_BURST && d.desired_kbps == 5120);
    return 0;
}

static int test_recovery_limits_and_retry(void)
{
    Fixture f;
    CHECK(warm(&f) == 0);
    X4AbrDecision d = tick(&f, 12, 90, 10, 0, 0, 0);
    CHECK(d.desired_kbps == 6400 && d.send_due);
    /* An unacknowledged reservation never creates duplicate sends. */
    d = tick(&f, 14, 100, 0, 0, 0, 0);
    CHECK(!d.send_due);
    x4_abr_note_send(&f.policy, 14000000, 6400, false);
    d = tick(&f, 16, 100, 0, 0, 0, 0);
    CHECK(d.send_due && d.send_kbps == 6400);
    x4_abr_note_send(&f.policy, 16000000, 6400, false);
    d = tick(&f, 18, 100, 0, 0, 0, 0);
    CHECK(d.send_due);
    x4_abr_note_send(&f.policy, 18000000, 6400, false);
    d = tick(&f, 20, 100, 0, 0, 0, 0);
    CHECK(!d.send_due && d.sent_kbps == 8000);
    for (uint64_t s = 22; s < 32; s += 2) (void)tick(&f, s, 100, 0, 0, 0, 0);
    d = tick(&f, 32, 100, 0, 0, 0, 0);
    CHECK(d.reason == X4_ABR_CLEAN_RECOVERY && d.desired_kbps == 6650);
    CHECK(d.send_due && d.send_kbps == 6650);
    x4_abr_note_send(&f.policy, 32000000, 6650, true);
    for (uint64_t s = 34; s < 52; s += 2) (void)tick(&f, s, 100, 0, 0, 0, 0);
    d = tick(&f, 52, 100, 0, 0, 0, 0);
    CHECK(d.reason == X4_ABR_CLEAN_RECOVERY && d.desired_kbps == 6900);
    x4_abr_note_send(&f.policy, 52000000, d.send_kbps, true);
    for (uint64_t s = 54; s <= 172; s += 2) {
        uint32_t before = f.policy.desired_kbps;
        d = tick(&f, s, 100, 0, 0, 0, 0);
        CHECK(d.desired_kbps >= before && d.desired_kbps - before <= 250);
        CHECK(d.desired_kbps <= 8000);
        if (d.send_due) x4_abr_note_send(&f.policy, s * 1000000u, d.send_kbps, true);
    }
    CHECK(f.policy.desired_kbps == 8000 && f.policy.sent_kbps == 8000);
    return 0;
}

static int test_floor_and_regressions(void)
{
    Fixture f;
    CHECK(warm(&f) == 0);
    for (uint64_t s = 12; s <= 62; s += 2) {
        X4AbrDecision d = tick(&f, s, 90, 10, 0, 0, 0);
        CHECK(d.desired_kbps >= 3000 && d.desired_kbps <= 8000);
        if (d.send_due) x4_abr_note_send(&f.policy, s * 1000000u, d.send_kbps, true);
    }
    CHECK(f.policy.desired_kbps == 3000);
    X4AbrDecision d = x4_abr_update(&f.policy, 61000000, &f.counters);
    CHECK(d.reason == X4_ABR_REGRESSION && d.desired_kbps == 3000);
    f.counters.valid_video_packets = 1;
    d = x4_abr_update(&f.policy, 63000000, &f.counters);
    CHECK(d.reason == X4_ABR_REGRESSION && d.desired_kbps == 3000);
    d = tick(&f, 65, 100, 0, 0, 0, 0);
    CHECK(d.reason == X4_ABR_WARMUP && d.desired_kbps == 3000);
    return 0;
}

static int test_overflow_and_gap(void)
{
    Fixture f;
    begin(&f);
    f.counters.valid_video_packets = UINT64_MAX;
    f.counters.received_video_bytes = UINT64_MAX;
    X4AbrDecision d = x4_abr_update(&f.policy, 2000000, &f.counters);
    CHECK(d.reason == X4_ABR_WARMUP && d.desired_kbps == 8000);
    CHECK(d.receive_kbps == UINT32_MAX);
    d = x4_abr_update(&f.policy, 10000000, &f.counters);
    CHECK(d.reason == X4_ABR_GAP && d.desired_kbps == 8000);
    d = x4_abr_update(&f.policy, 12000000, &f.counters);
    CHECK(d.reason == X4_ABR_EMPTY);
    return 0;
}

static int test_experimental_1080_loss_and_recovery(void)
{
    X4StreamProfile baseline = x4_stream_profile_get(X4_STREAM_720);
    X4StreamProfile profile = x4_stream_profile_get(X4_STREAM_1080);
    CHECK(baseline.h264_level == 32);
    CHECK(baseline.max_fs == 3600 && baseline.max_mbps == 216000);
    CHECK(strstr(baseline.h264_fmtp, "profile-level-id=42e020") != NULL);
    CHECK(((baseline.width + 15) / 16) * ((baseline.height + 15) / 16) <= baseline.max_fs);
    CHECK(((baseline.width + 15) / 16) * ((baseline.height + 15) / 16) *
        baseline.fps <= baseline.max_mbps);
    CHECK(profile.width == 1920 && profile.height == 1080 && profile.fps == 60);
    CHECK(profile.min_kbps == 5000 && profile.max_kbps == 15000);
    CHECK(profile.h264_level == 42);
    CHECK(((profile.width + 15) / 16) * ((profile.height + 15) / 16) <= profile.max_fs);
    CHECK(((profile.width + 15) / 16) * ((profile.height + 15) / 16) *
        profile.fps <= profile.max_mbps);
    CHECK(strstr(profile.h264_fmtp, "profile-level-id=42e02a") != NULL);
    CHECK(strstr(profile.h264_fmtp, "max-fs=8160;max-mbps=489600") != NULL);
    char caps[512], dims[384], short_buffer[8];
    CHECK(x4_stream_profile_capabilities(baseline, caps, sizeof(caps)) > 0);
    CHECK(strstr(caps, "\"maxWidth\":1280,\"maxHeight\":720,\"maxBitrateKbps\":8000") != NULL);
    CHECK(x4_stream_profile_capabilities(profile, caps, sizeof(caps)) > 0);
    CHECK(strstr(caps, "\"maxWidth\":1920,\"maxHeight\":1080,\"maxBitrateKbps\":15000") != NULL);
    CHECK(strstr(caps, "\"video\":{\"width\":1920,\"height\":1080") != NULL);
    CHECK(x4_stream_profile_dimensions(profile, dims, sizeof(dims)) > 0);
    CHECK(strstr(dims, "\"horizontal\":1920,\"vertical\":1080") != NULL);
    CHECK(strstr(dims, "\"safeAreaRight\":1920,\"safeAreaBottom\":1080") != NULL);
    CHECK(x4_stream_profile_capabilities(profile, short_buffer, sizeof(short_buffer)) < 0);
    CHECK(x4_stream_profile_dimensions(profile, short_buffer, sizeof(short_buffer)) < 0);
    Fixture f = {0};
    x4_abr_init_profile(&f.policy, 0, X4_STREAM_1080);
    (void)x4_abr_update(&f.policy, 0, &f.counters);
    for (uint64_t s = 2; s <= 10; s += 2) {
        X4AbrDecision d = tick(&f, s, 100, 0, 0, 0, 0);
        CHECK(d.desired_kbps == 15000);
        if (d.send_due) x4_abr_note_send(&f.policy, s * 1000000u, d.send_kbps, true);
    }
    for (uint64_t s = 12; s <= 62; s += 2) {
        X4AbrDecision d = tick(&f, s, 90, 10, 0, 0, 0);
        CHECK(d.desired_kbps >= 5000 && d.desired_kbps <= 15000);
        if (d.send_due) x4_abr_note_send(&f.policy, s * 1000000u, d.send_kbps, true);
    }
    CHECK(f.policy.desired_kbps == 5000);
    for (uint64_t s = 64; s <= 1064; s += 2) {
        X4AbrDecision d = tick(&f, s, 100, 0, 0, 0, 0);
        CHECK(d.desired_kbps >= 5000 && d.desired_kbps <= 15000);
        if (d.send_due) x4_abr_note_send(&f.policy, s * 1000000u, d.send_kbps, true);
    }
    CHECK(f.policy.desired_kbps == 15000);
    return 0;
}

int main(void)
{
    CHECK(test_empty_and_warmup() == 0);
    CHECK(test_false_congestion_and_receive_rate() == 0);
    CHECK(test_transient_sustained_and_hold_down() == 0);
    CHECK(test_recovery_limits_and_retry() == 0);
    CHECK(test_floor_and_regressions() == 0);
    CHECK(test_overflow_and_gap() == 0);
    CHECK(test_experimental_1080_loss_and_recovery() == 0);
    puts("adaptive_bitrate_test: all cases passed");
    return 0;
}
