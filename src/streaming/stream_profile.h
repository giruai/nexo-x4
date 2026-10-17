/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stddef.h>
#include <stdio.h>

/* A session copies one of these values before its worker starts. 1080p is
 * experimental: advertising it does not establish decoder/render capacity. */
typedef struct {
    unsigned width, height, fps;
    unsigned min_kbps, max_kbps;
    unsigned h264_level, max_fs, max_mbps;
    const char *h264_fmtp;
} X4StreamProfile;

enum { X4_STREAM_720 = 0, X4_STREAM_1080 = 1 };

static inline int x4_stream_profile_valid(int profile)
{
    return profile == X4_STREAM_720 || profile == X4_STREAM_1080;
}

static inline X4StreamProfile x4_stream_profile_get(int profile)
{
    if (profile == X4_STREAM_1080) {
        X4StreamProfile config = {
        1920, 1080, 60, 5000, 15000, 42, 8160, 489600,
        "level-asymmetry-allowed=0;packetization-mode=1;profile-level-id=42e02a;max-fs=8160;max-mbps=489600"
        };
        return config;
    }
    X4StreamProfile config = {
        1280, 720, 60, 3000, 8000, 32, 3600, 216000,
        "level-asymmetry-allowed=0;packetization-mode=1;profile-level-id=42e020;max-fs=3600;max-mbps=216000"
    };
    return config;
}

static inline int x4_stream_profile_capabilities(X4StreamProfile p,
    char *out, size_t capacity)
{
    if (!out || !capacity) return -1;
    int n = snprintf(out, capacity,
        "{\"supportsCustomResolution\":true,\"supportsHevc\":false,\"supportsHdr\":false,\"supportsFps\":%u,\"maxWidth\":%u,\"maxHeight\":%u,\"maxBitrateKbps\":%u,\"video\":{\"width\":%u,\"height\":%u,\"maxWidth\":%u,\"maxHeight\":%u,\"maxBitrateKbps\":%u}}",
        p.fps, p.width, p.height, p.max_kbps, p.width, p.height,
        p.width, p.height, p.max_kbps);
    return n >= 0 && (size_t)n < capacity ? n : -1;
}

static inline int x4_stream_profile_dimensions(X4StreamProfile p,
    char *out, size_t capacity)
{
    if (!out || !capacity) return -1;
    int n = snprintf(out, capacity,
        "{\"horizontal\":%u,\"vertical\":%u,\"preferredWidth\":%u,\"preferredHeight\":%u,\"safeAreaLeft\":0,\"safeAreaTop\":0,\"safeAreaRight\":%u,\"safeAreaBottom\":%u,\"supportsCustomResolution\":true}",
        p.width, p.height, p.width, p.height, p.width, p.height);
    return n >= 0 && (size_t)n < capacity ? n : -1;
}

/* Explicit control-plane request used by the Xbox streaming protocol. This
 * asks for a base resolution only; it neither grants a paid tier nor proves
 * that the service delivered the requested dimensions. */
static inline int x4_stream_profile_resolution_message(X4StreamProfile p,
    char *out, size_t capacity)
{
    if (!out || !capacity) return -1;
    out[0] = 0;
    const char *alias = p.width == 1920 && p.height == 1080 ? "1080" :
        p.width == 1280 && p.height == 720 ? "720" : NULL;
    if (!alias) return -1;
    int n = snprintf(out, capacity,
        "{\"message\":\"userRequestedResolutionUpdate\",\"resolutionAlias\":\"%s\"}", alias);
    if (n < 0 || (size_t)n >= capacity) { out[0] = 0; return -1; }
    return n;
}
