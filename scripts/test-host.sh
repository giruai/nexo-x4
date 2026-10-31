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
for name in region_selection stream_controls session_store_import; do
    extra=()
    case "$name" in
        region_selection) sources=(src/auth/region_selection.c src/auth/json.c) ;;
        stream_controls) sources=(src/input/stream_controls.c) ;;
        session_store_import) sources=(src/auth/session_store.c); extra=(-D_POSIX_C_SOURCE=200809L) ;;
    esac
    "$host_cc" "${flags[@]}" "${extra[@]}" "tests/${name}_test.c" "${sources[@]}" -o ".local/tests/$name"
    ".local/tests/$name"
done
"$host_cc" "${flags[@]}" -c src/auth/region_selection.c -o .local/tests/region_selection.o
"$host_cc" "${flags[@]}" -c src/auth/json.c -o .local/tests/json.o
host_cxx=${HOST_CXX:-clang++}
cppflags=(-O1 -g -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer)
"$host_cxx" "${cppflags[@]}" -msse2 tests/overlay_copy_test.cpp -o .local/tests/overlay_copy
.local/tests/overlay_copy
"$host_cxx" "${cppflags[@]}" tests/psbox_prefs_test.cpp src/ui/psbox/prefs.cpp .local/tests/region_selection.o .local/tests/json.o -o .local/tests/psbox_prefs
.local/tests/psbox_prefs
# Reuse vendor objects across the three UI fixtures; AppUi is expensive under
# amd64 translation and every fixture uses the same sanitizer configuration.
ui_flags=(-msse2 -Wno-unused-parameter -Wno-unused-private-field -DX4_OPENORBIS -Isrc/ui/psbox/vendor -isystem src/ui/psbox/vendor/extern)
ui_objects=()
for unit in app_ui stream_menu canvas font strings pad_icons accent_color brand; do
    object=".local/tests/ui_${unit}.o"
    "$host_cxx" "${cppflags[@]}" "${ui_flags[@]}" -c "src/ui/psbox/vendor/ui/${unit}.cpp" -o "$object"
    ui_objects+=("$object")
done
"$host_cxx" "${cppflags[@]}" "${ui_flags[@]}" -c src/ui/psbox/vendor/input/key_map.cpp -o .local/tests/ui_key_map.o
"$host_cc" "${flags[@]}" -c src/ui/psbox/vendor/extern/qrcodegen.c -o .local/tests/qrcodegen.o
ui_objects+=(.local/tests/ui_key_map.o .local/tests/qrcodegen.o)
for name in stream_menu ps4_settings; do
    fixture_objects=("${ui_objects[@]}")
    if [[ $name == stream_menu ]]; then
        fixture_objects=(.local/tests/ui_stream_menu.o .local/tests/ui_canvas.o .local/tests/ui_font.o .local/tests/ui_strings.o .local/tests/ui_pad_icons.o)
    fi
    "$host_cxx" "${cppflags[@]}" "${ui_flags[@]}" "tests/${name}_test.cpp" "${fixture_objects[@]}" -o ".local/tests/$name"
    ".local/tests/$name" assets/psbox/fonts
done
for unit in game_text canvas; do
    "$host_cc" "${flags[@]}" -Itests/host_stubs -c "src/ui/${unit}.c" -o ".local/tests/core_${unit}.o"
done
"$host_cc" "${flags[@]}" -c src/auth/status_copy.c -o .local/tests/status_copy.o
"$host_cxx" "${cppflags[@]}" "${ui_flags[@]}" tests/psbox_catalog_refresh_test.cpp src/ui/psbox/prefs.cpp \
    "${ui_objects[@]}" .local/tests/core_game_text.o .local/tests/core_canvas.o \
    .local/tests/status_copy.o .local/tests/region_selection.o .local/tests/json.o -o .local/tests/psbox_catalog_refresh
.local/tests/psbox_catalog_refresh assets/psbox/fonts
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
