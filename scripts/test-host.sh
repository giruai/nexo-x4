#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
source "$(dirname "$0")/common.sh"
cd "$NEXO_ROOT"
mkdir -p .local/tests
host_cc=${HOST_CC:-clang}
flags=(-O1 -g -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer)
for name in adaptive_bitrate video_feedback live_scale; do
    "$host_cc" "${flags[@]}" -msse2 "tests/${name}_test.c" -o ".local/tests/$name"
    ".local/tests/$name"
done
"$host_cc" "${flags[@]}" -Isrc/streaming tests/receiver_nack_state_test.c src/streaming/receiver_nack_state.c -o .local/tests/receiver_nack_state
.local/tests/receiver_nack_state
"$host_cc" "${flags[@]}" -pthread -DX4_INGRESS_HOST_TEST -DX4_INGRESS_TEST_HOOKS -Isrc/media tests/video_ingress_test.c src/media/video_ingress.c -o .local/tests/video_ingress
.local/tests/video_ingress
printf 'Portable host checks passed. These do not emulate PS4 video hardware.\n'
