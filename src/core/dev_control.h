/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stdbool.h>
#include <stdint.h>

#if X4_DEV_CONTROL
typedef struct {
    uint64_t last_poll_us;
    uint64_t last_seq;
    uint64_t capture_seq;
    int capture_pending;
    unsigned abr_action; /* 1=4Mbps probe, 2=8Mbps probe, 3=auto, 4=8s loss probe */
} X4DevControl;

/* Main thread only. The FTP client publishes command.txt by atomic rename. */
void x4_dev_control_poll(X4DevControl *control, uint64_t now_us, int page, bool busy,
    uint32_t *pressed, uint32_t *held);
/* Call after drawing the catalog and before submitting that framebuffer. */
void x4_dev_control_capture(X4DevControl *control, int page, const uint32_t *pixels);
#endif
