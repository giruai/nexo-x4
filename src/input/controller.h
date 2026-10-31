/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <orbis/Pad.h>
#include "gamepad.h"
#include "stream_controls.h"
typedef struct {
    int handle;
    int library_ready;
    int error;
    uint32_t previous;
    uint32_t pressed;
    uint16_t touch_resolution_x, touch_resolution_y;
    OrbisPadData data;
} X4Controller;
void x4_controller_init(X4Controller *controller);
void x4_controller_read(X4Controller *controller, unsigned frame);
void x4_controller_close(X4Controller *controller);
/* Xbox axes use positive Y upwards; no private SDK button masks are guessed. */
void x4_controller_gamepad(const X4Controller *controller, X4GamepadFrame *out);
/* Device fields only; caller sets streaming/text_capture/menu_open. Geometry
 * comes from public scePadGetControllerInformation, never a guessed pad size. */
void x4_controller_stream_sample(const X4Controller *controller, uint32_t now_ms,
    X4StreamControlsSample *out);
