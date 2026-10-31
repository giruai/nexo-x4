/* SPDX-License-Identifier: GPL-3.0-only */
/* Initialization follows the public OpenOrbis input sample. */
#include "controller.h"
#include "diagnostics.h"
#include <stddef.h>
#include <string.h>
#if X4_INPUT_TRACE
#include <stdio.h>
#endif
#include <orbis/UserService.h>
/* Public OpenOrbis _types/pad.h touch ABI. In particular, finger is a contact
 * identity; it is not inferred from the touch-array index or button mask. */
_Static_assert(sizeof(OrbisPadTouch) == 8, "OpenOrbis touch ABI");
_Static_assert(offsetof(OrbisPadTouch, x) == 0 && offsetof(OrbisPadTouch, y) == 2 &&
    offsetof(OrbisPadTouch, finger) == 4, "OpenOrbis touch fields");
_Static_assert(offsetof(OrbisPadTouchData, touch) == 8, "OpenOrbis touch array");

static void read_touch_geometry(X4Controller *c)
{
    OrbisPadInformation info = {0};
    c->touch_resolution_x = c->touch_resolution_y = 0;
    if (c->handle >= 0 && scePadGetControllerInformation(c->handle, &info) >= 0 &&
        info.touchResolutionX > 1 && info.touchResolutionY > 1) {
        c->touch_resolution_x = info.touchResolutionX;
        c->touch_resolution_y = info.touchResolutionY;
    }
}

static void open_controller(X4Controller *c)
{
    int rc;
    if (!c->library_ready) {
        rc = scePadInit();
        if (rc < 0) { c->error = rc; return; }
        c->library_ready = 1;
    }
    OrbisUserServiceInitializeParams params = {.priority = ORBIS_KERNEL_PRIO_FIFO_LOWEST};
    sceUserServiceInitialize(&params);
    int32_t user = -1;
    rc = sceUserServiceGetInitialUser(&user);
    if (rc < 0) { c->error = rc; return; }
    rc = scePadOpen(user, ORBIS_PAD_PORT_TYPE_STANDARD, 0, NULL);
    if (rc < 0) { c->error = rc; return; }
    c->handle = rc;
    c->error = 0;
    read_touch_geometry(c);
}
void x4_controller_init(X4Controller *c)
{
    memset(c, 0, sizeof(*c)); c->handle = -1; open_controller(c);
}
void x4_controller_read(X4Controller *c, unsigned frame)
{
    c->pressed = 0;
    if (c->handle < 0 && frame % 120 == 0) open_controller(c);
    OrbisPadData data = {0};
    data.leftStick.x = data.leftStick.y = 128;
    data.rightStick.x = data.rightStick.y = 128;
    int rc = c->handle < 0 ? c->error : scePadReadState(c->handle, &data);
    if (rc < 0) {
        c->error = rc; data.connected = 0; data.buttons = 0;
        if (c->handle >= 0) scePadClose(c->handle);
        c->handle = -1;
    } else c->error = 0;
    if (!data.connected) {
        data.buttons = 0;
        data.leftStick.x = data.leftStick.y = 128;
        data.rightStick.x = data.rightStick.y = 128;
        data.analogButtons.l2 = data.analogButtons.r2 = 0;
        c->touch_resolution_x = c->touch_resolution_y = 0;
    } else if (!c->data.connected || (!c->touch_resolution_x && frame % 120 == 0)) {
        read_touch_geometry(c);
    }
    c->pressed = data.buttons & ~c->previous;
    c->previous = data.buttons;
    c->data = data;
#if X4_INPUT_TRACE
    /* Numeric, bounded trigger-only evidence; never log text or account data. */
    static uint32_t trace_previous = UINT32_MAX;
    static unsigned trace_count;
    uint32_t trace_value = data.analogButtons.l2 | ((uint32_t)data.analogButtons.r2 << 8) |
        ((data.buttons & (ORBIS_PAD_BUTTON_L2 | ORBIS_PAD_BUTTON_R2)) << 16);
    if (!x4_input_diagnostics_enabled()) { trace_previous = UINT32_MAX; trace_count = 0; }
    else if (trace_value != trace_previous && trace_count < 64) {
        printf("Nexo input raw: connected=%u rc=%d l2=%u r2=%u l2bit=%u r2bit=%u\n",
            (unsigned)data.connected, rc, (unsigned)data.analogButtons.l2,
            (unsigned)data.analogButtons.r2, !!(data.buttons & ORBIS_PAD_BUTTON_L2),
            !!(data.buttons & ORBIS_PAD_BUTTON_R2));
        trace_previous = trace_value; ++trace_count;
    }
#endif
}
void x4_controller_close(X4Controller *c)
{
    if (c->handle >= 0) scePadClose(c->handle);
    c->handle = -1;
    c->touch_resolution_x = c->touch_resolution_y = 0;
    c->data.connected = 0;
    c->data.buttons = c->previous = c->pressed = 0;
}

static int16_t axis(uint8_t value)
{
    int n = (int)value - 128;
    return (int16_t)(n >= 0 ? n * 32767 / 127 : n * 32767 / 128);
}

void x4_controller_gamepad(const X4Controller *c, X4GamepadFrame *out)
{
    if (!out) return;
    memset(out, 0, sizeof(*out));
    if (!c || !c->data.connected || c->error) return;
    out->connected = true;
    static const struct { uint32_t ps; uint16_t xbox; } map[] = {
        {ORBIS_PAD_BUTTON_CROSS, X4_GAMEPAD_A}, {ORBIS_PAD_BUTTON_CIRCLE, X4_GAMEPAD_B},
        {ORBIS_PAD_BUTTON_SQUARE, X4_GAMEPAD_X}, {ORBIS_PAD_BUTTON_TRIANGLE, X4_GAMEPAD_Y},
        {ORBIS_PAD_BUTTON_OPTIONS, X4_GAMEPAD_MENU}, {ORBIS_PAD_BUTTON_TOUCH_PAD, X4_GAMEPAD_VIEW},
        {ORBIS_PAD_BUTTON_UP, X4_GAMEPAD_UP}, {ORBIS_PAD_BUTTON_DOWN, X4_GAMEPAD_DOWN},
        {ORBIS_PAD_BUTTON_LEFT, X4_GAMEPAD_LEFT}, {ORBIS_PAD_BUTTON_RIGHT, X4_GAMEPAD_RIGHT},
        {ORBIS_PAD_BUTTON_L1, X4_GAMEPAD_LB}, {ORBIS_PAD_BUTTON_R1, X4_GAMEPAD_RB},
        {ORBIS_PAD_BUTTON_L3, X4_GAMEPAD_L3}, {ORBIS_PAD_BUTTON_R3, X4_GAMEPAD_R3},
    };
    for (size_t i = 0; i < sizeof(map) / sizeof(map[0]); ++i)
        if (c->data.buttons & map[i].ps) out->buttons |= map[i].xbox;
    out->left_x = axis(c->data.leftStick.x);
    out->left_y = (int16_t)-axis(c->data.leftStick.y);
    out->right_x = axis(c->data.rightStick.x);
    out->right_y = (int16_t)-axis(c->data.rightStick.y);
    out->left_trigger = (uint16_t)c->data.analogButtons.l2 * 257u;
    out->right_trigger = (uint16_t)c->data.analogButtons.r2 * 257u;
}

void x4_controller_stream_sample(const X4Controller *c, uint32_t now_ms,
    X4StreamControlsSample *out)
{
    if (!out) return;
    memset(out, 0, sizeof(*out));
    out->now_ms = now_ms;
    out->analog_neutral = true;
    if (!c || !c->data.connected || c->error) return;
    const OrbisPadData *pad = &c->data;
    out->connected = true;
    out->buttons = pad->buttons;
    out->analog_neutral = pad->analogButtons.l2 <= 8 && pad->analogButtons.r2 <= 8 &&
        pad->leftStick.x >= 108 && pad->leftStick.x <= 148 &&
        pad->leftStick.y >= 108 && pad->leftStick.y <= 148 &&
        pad->rightStick.x >= 108 && pad->rightStick.x <= 148 &&
        pad->rightStick.y >= 108 && pad->rightStick.y <= 148;
    out->touch.count = pad->touch.fingers;
    out->touch.valid = c->touch_resolution_x > 1 && c->touch_resolution_y > 1;
    if (out->touch.valid && out->touch.count == 1) {
        const OrbisPadTouch *t = &pad->touch.touch[0];
        if (t->x >= c->touch_resolution_x || t->y >= c->touch_resolution_y) {
            out->touch.valid = false; /* Corrupt geometry/sample cancels contact. */
            return;
        }
        out->touch.id = t->finger;
        out->touch.x = (uint16_t)((uint32_t)t->x * UINT16_MAX / (c->touch_resolution_x - 1u));
        out->touch.y = (uint16_t)((uint32_t)t->y * UINT16_MAX / (c->touch_resolution_y - 1u));
    }
}
