/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stdbool.h>
#include <stdint.h>

/* Device-independent, normalized touch coordinates. A provider without valid
 * touch geometry reports valid=false, count=0; physical pad buttons still work.
 * count>1 deliberately cancels the complete contact sequence. */
typedef struct {
    bool valid;
    uint8_t count, id;
    uint16_t x, y; /* [0,65535], Y increases downwards. */
} X4StreamTouch;

typedef struct {
    uint32_t buttons, now_ms;
    bool connected, streaming, text_capture, menu_open, analog_neutral;
    X4StreamTouch touch;
} X4StreamControlsSample;

typedef struct {
    uint32_t touchpad_button;
    /* Use a chord without Options/View: both keep their immediate remote
     * semantics. A chord containing either cannot hide its leading prefix
     * without delaying the standalone button. Zero disables the fallback. */
    uint32_t menu_chord;
} X4StreamControlsConfig;

enum X4StreamControlAction {
    X4_STREAM_CONTROL_NONE,
    X4_STREAM_CONTROL_GUIDE,
    X4_STREAM_CONTROL_MENU
};
typedef struct {
    enum X4StreamControlAction action;
    uint32_t consume_buttons;
    /* Send a neutral remote frame (with the actual connected flag). The
     * caller may overlay its bounded Guide pulse after routing this frame. */
    bool capture_remote;
} X4StreamControlsResult;

typedef struct {
    bool active, waiting_neutral, chord_armed, touch_blocked, touching;
    uint8_t touch_id;
    uint16_t start_x, start_y, last_y;
    uint32_t touch_start_ms;
} X4StreamControls;

void x4_stream_controls_reset(X4StreamControls *state);
X4StreamControlsResult x4_stream_controls_step(X4StreamControls *state,
    const X4StreamControlsSample *sample, const X4StreamControlsConfig *config);
