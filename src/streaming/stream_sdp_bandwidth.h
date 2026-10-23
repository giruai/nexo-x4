/* SPDX-License-Identifier: GPL-3.0-only */
/* Adapted from XCloud4, commit 008f4abe4f777fafa4259fe081ebfab065eed2d5. */
#pragma once
#include <stddef.h>
/* Only media-level video b=AS changes. Every audio/application/session line
 * and every secret/candidate byte is preserved without inspecting its value.
 * Caller owns a NUL-terminated bounded buffer. Failure leaves it unchanged. */
int x4_stream_sdp_limit_video(char *sdp,size_t capacity,unsigned kbps);
