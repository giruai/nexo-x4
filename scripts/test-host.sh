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
"$host_cc" "${flags[@]}" tests/queue_estimate_test.c src/auth/queue_estimate.c src/auth/json.c -o .local/tests/queue_estimate
.local/tests/queue_estimate
"$host_cc" "${flags[@]}" tests/stream_profile_test.c src/auth/json.c -o .local/tests/stream_profile
.local/tests/stream_profile
"$host_cc" "${flags[@]}" tests/text_input_test.c src/streaming/text_input.c src/auth/json.c -o .local/tests/text_input
.local/tests/text_input
"$host_cc" "${flags[@]}" -Itests/host_stubs tests/game_text_test.c src/ui/game_text.c src/ui/canvas.c -o .local/tests/game_text
.local/tests/game_text
"$host_cc" "${flags[@]}" tests/status_copy_test.c src/auth/status_copy.c -o .local/tests/status_copy
.local/tests/status_copy
"$host_cc" "${flags[@]}" tests/public_metadata_locale_test.c -o .local/tests/public_metadata_locale
.local/tests/public_metadata_locale
host_cxx=${HOST_CXX:-clang++}
cppflags=(-O1 -g -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer)
"$host_cxx" "${cppflags[@]}" -msse2 tests/overlay_copy_test.cpp -o .local/tests/overlay_copy
.local/tests/overlay_copy
"$host_cxx" "${cppflags[@]}" tests/psbox_prefs_test.cpp src/ui/psbox/prefs.cpp -o .local/tests/psbox_prefs
.local/tests/psbox_prefs
"$host_cxx" "${cppflags[@]}" -DX4_OPENORBIS -Isrc/ui/psbox/vendor -isystem src/ui/psbox/vendor/extern \
    tests/stream_menu_test.cpp src/ui/psbox/vendor/ui/stream_menu.cpp src/ui/psbox/vendor/ui/canvas.cpp \
    src/ui/psbox/vendor/ui/font.cpp src/ui/psbox/vendor/ui/strings.cpp src/ui/psbox/vendor/ui/pad_icons.cpp \
    -o .local/tests/stream_menu
.local/tests/stream_menu assets/psbox/fonts
"$host_cxx" "${cppflags[@]}" tests/queue_view_test.cpp -o .local/tests/queue_view
.local/tests/queue_view
"$host_cc" "${flags[@]}" tests/stream_sdp_bandwidth_test.c src/streaming/stream_sdp_bandwidth.c -o .local/tests/stream_sdp_bandwidth
.local/tests/stream_sdp_bandwidth
"$host_cc" "${flags[@]}" -pthread -Itests/audio_stubs tests/live_audio_modes_test.c -o .local/tests/live_audio_modes
.local/tests/live_audio_modes
"$host_cc" "${flags[@]}" -msse2 -Itests/decoder_stubs tests/live_h264_test.c src/video/stream_sps.c -o .local/tests/live_h264
.local/tests/live_h264
"$host_cc" "${flags[@]}" tests/decode_recovery_test.c -o .local/tests/decode_recovery
.local/tests/decode_recovery
"$host_cc" "${flags[@]}" tests/conversion_queue_test.c -o .local/tests/conversion_queue
.local/tests/conversion_queue
"$host_cc" "${flags[@]}" -pthread -msse2 -Itests/decoder_stubs tests/live_h264_async_test.c src/video/stream_sps.c -o .local/tests/live_h264_async
.local/tests/live_h264_async
printf 'Portable host checks passed. These do not emulate PS4 video hardware.\n'
