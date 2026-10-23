/* SPDX-License-Identifier: GPL-3.0-only */
#include "../src/streaming/stream_sdp_bandwidth.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void unchanged_failure(const char *source, size_t cap)
{
    char sdp[512], before[512];
    memset(sdp, 0xa5, sizeof(sdp));
    memcpy(sdp, source, strlen(source) + 1);
    memcpy(before, sdp, sizeof(sdp));
    assert(x4_stream_sdp_limit_video(sdp, cap, 8000) < 0);
    assert(memcmp(sdp, before, sizeof(sdp)) == 0);
}
int main(void)
{
    char sdp[2048];
    const char *source = "v=0\r\nb=AS:15000\r\nm=audio 9 RTP/SAVPF 111\r\nb=AS:128\r\na=ice-pwd:audio-private\r\n"
        "m=video 9 RTP/SAVPF 102\r\ni=video\r\nc=IN IP4 0.0.0.0\r\nb=AS:15000\r\na=recvonly\r\nb=AS:broken\r\n"
        "a=ice-pwd:video-private\r\na=candidate:private candidate bytes\r\n"
        "m=application 9 UDP/DTLS/SCTP webrtc-datachannel\r\nb=AS:256\r\na=sctp-port:5000\r\n";
    const char *expected = "v=0\r\nb=AS:15000\r\nm=audio 9 RTP/SAVPF 111\r\nb=AS:128\r\na=ice-pwd:audio-private\r\n"
        "m=video 9 RTP/SAVPF 102\r\ni=video\r\nc=IN IP4 0.0.0.0\r\nb=AS:8000\r\na=recvonly\r\n"
        "a=ice-pwd:video-private\r\na=candidate:private candidate bytes\r\n"
        "m=application 9 UDP/DTLS/SCTP webrtc-datachannel\r\nb=AS:256\r\na=sctp-port:5000\r\n";
    strcpy(sdp, source);
    assert(x4_stream_sdp_limit_video(sdp, sizeof(sdp), 8000) == 0);
    assert(!strcmp(sdp, expected));
    assert(x4_stream_sdp_limit_video(sdp, sizeof(sdp), 8000) == 0);
    assert(!strcmp(sdp, expected)); /* Idempotent; no duplicate AS. */
    strcpy(sdp, "v=0\nm=video 9 RTP/SAVPF 102\nc=IN IP4 0.0.0.0\na=recvonly\nm=audio 9 RTP/SAVPF 111\nb=AS:128\nm=video 9 RTP/SAVPF 103");
    assert(x4_stream_sdp_limit_video(sdp, sizeof(sdp), 8000) == 0);
    assert(!strcmp(sdp, "v=0\nm=video 9 RTP/SAVPF 102\nc=IN IP4 0.0.0.0\nb=AS:8000\na=recvonly\nm=audio 9 RTP/SAVPF 111\nb=AS:128\nm=video 9 RTP/SAVPF 103\nb=AS:8000\n"));
    strcpy(sdp, "m=video 9 RTP/SAVPF 102\na=recvonly");
    assert(x4_stream_sdp_limit_video(sdp, sizeof(sdp), 8000) == 0);
    assert(!strcmp(sdp, "m=video 9 RTP/SAVPF 102\nb=AS:8000\na=recvonly"));
    unchanged_failure("v=0\nm=audio 9 RTP/SAVPF 111\nb=AS:128\n", sizeof(sdp)/4);
    unchanged_failure("v=0\nm=videographic 9 RTP/SAVPF 102\n", 512);
    unchanged_failure("m=video 9 RTP/SAVPF 102\na=recvonly\n", 33); /* no terminator */
    const char *plain = "m=video 9 RTP/SAVPF 102\na=recvonly\n";
    unchanged_failure(plain, strlen(plain) + 1); /* insertion won't fit */
    strcpy(sdp, plain);
    size_t exact = strlen(plain) + strlen("b=AS:8000\n") + 1;
    assert(x4_stream_sdp_limit_video(sdp, exact, 8000) == 0);
    assert(strlen(sdp) + 1 == exact);
    strcpy(sdp, plain);
    assert(x4_stream_sdp_limit_video(sdp, sizeof(sdp), 0) < 0 && !strcmp(sdp, plain));
    assert(x4_stream_sdp_limit_video(NULL, sizeof(sdp), 8000) < 0);
    assert(x4_stream_sdp_limit_video(sdp, 0, 8000) < 0);
    puts("stream_sdp_bandwidth_test: video-only insertion, replacement and bounds passed");
}
