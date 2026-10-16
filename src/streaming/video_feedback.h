/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/* Inspect only the accepted H264 video media section. A feedback attribute
 * belonging to audio, a different payload or a rejected section is not a
 * negotiated permission to send video REMB. No SDP text is logged. */
static inline bool x4_video_remb_negotiated(const char *sdp, size_t length, unsigned payload)
{
    if (!sdp || !length || length >= 32768 || payload > 127) return false;
    bool video = false, active = false, codec = false, remb = false;
    for (size_t off = 0; off < length;) {
        size_t end = off;
        while (end < length && sdp[end] != '\n') ++end;
        size_t n = end - off;
        if (n && sdp[off + n - 1] == '\r') --n;
        const char *p = sdp + off;
        off = end < length ? end + 1 : end;
        if (n >= 2 && !memcmp(p, "m=", 2)) {
            if (video && active && codec && remb) return true;
            video = n > 8 && !memcmp(p, "m=video ", 8);
            active = video && p[8] >= '1' && p[8] <= '9';
            codec = remb = false;
            continue;
        }
        if (!video) continue;
        if (n == 10 && !memcmp(p, "a=inactive", 10)) { active = false; continue; }
        bool map = n > 9 && !memcmp(p, "a=rtpmap:", 9);
        bool fb = n > 10 && !memcmp(p, "a=rtcp-fb:", 10);
        if (!map && !fb) continue;
        size_t at = map ? 9 : 10;
        bool wildcard = fb && p[at] == '*';
        unsigned pt = 0, digits = 0;
        if (wildcard) ++at;
        else while (at < n && p[at] >= '0' && p[at] <= '9') {
            if (++digits > 3) break;
            pt = pt * 10 + (unsigned)(p[at++] - '0');
        }
        if ((!wildcard && (!digits || pt != payload)) || at >= n || (p[at] != ' ' && p[at] != '\t')) continue;
        while (at < n && (p[at] == ' ' || p[at] == '\t')) ++at;
        if (map && n - at == 10 && !memcmp(p + at, "H264/90000", 10)) codec = true;
        if (fb && n - at == 9 && !memcmp(p + at, "goog-remb", 9)) remb = true;
    }
    return video && active && codec && remb;
}
