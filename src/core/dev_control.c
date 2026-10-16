/* SPDX-License-Identifier: GPL-3.0-only */
#include "dev_control.h"

#if X4_DEV_CONTROL
#include "../video/display.h"
#include <orbis/Pad.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define X4_DEV_DIR "/data/xcloud4-dev/"
#define X4_DEV_POLL_US 100000ull

static bool enabled(void)
{
    FILE *f = fopen(X4_DEV_DIR "enabled", "rb");
    if (!f) return false;
    fclose(f);
    return true;
}

static void status(uint64_t seq, const char *result)
{
    FILE *f = fopen(X4_DEV_DIR "status.txt.partial", "wb");
    if (!f) return;
    int ok = fprintf(f, "%llu %s\n", (unsigned long long)seq, result) > 0;
    ok = fflush(f) == 0 && ok;
    ok = fclose(f) == 0 && ok;
    if (ok) (void)rename(X4_DEV_DIR "status.txt.partial", X4_DEV_DIR "status.txt");
    else (void)remove(X4_DEV_DIR "status.txt.partial");
}

/* Exactly: unsigned decimal sequence, one space, fixed action, newline or EOF. */
static bool parse(const char *text, size_t size, uint64_t *seq, const char **action)
{
    size_t i = 0;
    uint64_t value = 0;
    if (!size || text[0] < '0' || text[0] > '9') return false;
    while (i < size && text[i] >= '0' && text[i] <= '9') {
        unsigned digit = (unsigned)(text[i++] - '0');
        if (value > (UINT64_MAX - digit) / 10) return false;
        value = value * 10 + digit;
    }
    if (!value || i >= size || text[i++] != ' ') return false;
    size_t start = i;
    while (i < size && ((text[i] >= 'a' && text[i] <= 'z') ||
        (text[i] >= '0' && text[i] <= '9'))) ++i;
    size_t end = i;
    if (end == start || (i < size && (text[i++] != '\n' || i != size))) return false;
    static const char *const actions[] = {
        "cross", "circle", "square", "triangle", "l1", "r1", "up", "down",
        "left", "right", "options", "r3", "l2", "r2", "close", "screenshot",
        "abrlow", "abrhigh", "abrauto", "abrloss"
    };
    for (size_t n = 0; n < sizeof(actions) / sizeof(actions[0]); ++n) {
        if (strlen(actions[n]) == end - start && !memcmp(text + start, actions[n], end - start)) {
            *seq = value;
            *action = actions[n];
            return true;
        }
    }
    return false;
}

void x4_dev_control_poll(X4DevControl *control, uint64_t now_us, int page, bool busy,
    uint32_t *pressed, uint32_t *held)
{
    if (!control || !pressed || !held) return;
    if (control->last_poll_us && now_us - control->last_poll_us < X4_DEV_POLL_US) return;
    control->last_poll_us = now_us;
    if (!enabled()) return;
    if (rename(X4_DEV_DIR "command.txt", X4_DEV_DIR "command.processing") != 0) return;
    FILE *f = fopen(X4_DEV_DIR "command.processing", "rb");
    char command[64] = {0};
    size_t size = f ? fread(command, 1, sizeof(command), f) : 0;
    int eof = f ? feof(f) : 0;
    if (f) fclose(f);
    (void)remove(X4_DEV_DIR "command.processing");
    uint64_t seq = 0;
    const char *action = NULL;
    if (!eof || !parse(command, size, &seq, &action)) { status(0, "invalid"); return; }
    if (seq <= control->last_seq) { status(seq, "stale"); return; }
    control->last_seq = seq;
    if (!strncmp(action, "abr", 3)) {
        if (page != 6 || !busy) { status(seq, "refused-page"); return; }
        control->abr_action = !strcmp(action, "abrlow") ? 1u :
            !strcmp(action, "abrhigh") ? 2u : !strcmp(action, "abrauto") ? 3u : 4u;
        status(seq, "abr-queued");
        return;
    }
    if (!strcmp(action, "screenshot")) {
        if (page != 5 && page != 0x105) { status(seq, "refused-page"); return; }
        control->capture_seq = seq;
        control->capture_pending = 1;
        return;
    }
    /* 0x105 is the PSBox frontend while the actual display page remains 5.
     * Close uses the app's normal exit chord, including worker cancellation. */
    if (!strcmp(action, "close")) {
        if (page != 0x105) { status(seq, "refused-page"); return; }
        if (*pressed || *held) { status(seq, "physical-input"); return; }
        *pressed |= ORBIS_PAD_BUTTON_OPTIONS;
        *held |= ORBIS_PAD_BUTTON_OPTIONS | ORBIS_PAD_BUTTON_L1 | ORBIS_PAD_BUTTON_R1;
        status(seq, "injected");
        return;
    }
    if (busy) { status(seq, "busy"); return; }
    if (*pressed || *held) { status(seq, "physical-input"); return; }
    uint32_t button = 0;
    if (page == 0) {
        if (!strcmp(action, "cross")) button = ORBIS_PAD_BUTTON_CROSS;
        else if (!strcmp(action, "up")) button = ORBIS_PAD_BUTTON_UP;
        else if (!strcmp(action, "down")) button = ORBIS_PAD_BUTTON_DOWN;
        else if (!strcmp(action, "left")) button = ORBIS_PAD_BUTTON_LEFT;
        else if (!strcmp(action, "right")) button = ORBIS_PAD_BUTTON_RIGHT;
    } else if (page >= 1 && page <= 3) {
        if (!strcmp(action, "circle")) button = ORBIS_PAD_BUTTON_CIRCLE;
    } else if (page == 4) {
        if (!strcmp(action, "r1")) button = ORBIS_PAD_BUTTON_R1;
    } else if (page == 0x105) {
        if (!strcmp(action, "cross")) button = ORBIS_PAD_BUTTON_CROSS;
        else if (!strcmp(action, "circle")) button = ORBIS_PAD_BUTTON_CIRCLE;
        else if (!strcmp(action, "square")) button = ORBIS_PAD_BUTTON_SQUARE;
        else if (!strcmp(action, "triangle")) button = ORBIS_PAD_BUTTON_TRIANGLE;
        else if (!strcmp(action, "options")) button = ORBIS_PAD_BUTTON_OPTIONS;
        else if (!strcmp(action, "up")) button = ORBIS_PAD_BUTTON_UP;
        else if (!strcmp(action, "down")) button = ORBIS_PAD_BUTTON_DOWN;
        else if (!strcmp(action, "left")) button = ORBIS_PAD_BUTTON_LEFT;
        else if (!strcmp(action, "right")) button = ORBIS_PAD_BUTTON_RIGHT;
        else if (!strcmp(action, "l1")) button = ORBIS_PAD_BUTTON_L1;
        else if (!strcmp(action, "r1")) button = ORBIS_PAD_BUTTON_R1;
        else if (!strcmp(action, "r3")) button = ORBIS_PAD_BUTTON_R3;
        else if (!strcmp(action, "l2")) button = ORBIS_PAD_BUTTON_L2;
        else if (!strcmp(action, "r2")) button = ORBIS_PAD_BUTTON_R2;
    } else if (page == 5) {
        if (!strcmp(action, "square")) button = ORBIS_PAD_BUTTON_SQUARE;
        else if (!strcmp(action, "circle")) button = ORBIS_PAD_BUTTON_CIRCLE;
        else if (!strcmp(action, "up")) button = ORBIS_PAD_BUTTON_UP;
        else if (!strcmp(action, "down")) button = ORBIS_PAD_BUTTON_DOWN;
        else if (!strcmp(action, "left")) button = ORBIS_PAD_BUTTON_LEFT;
        else if (!strcmp(action, "right")) button = ORBIS_PAD_BUTTON_RIGHT;
        else if (!strcmp(action, "l1")) button = ORBIS_PAD_BUTTON_L1;
        else if (!strcmp(action, "r1")) button = ORBIS_PAD_BUTTON_R1;
    }
    if (!button) { status(seq, "refused-page"); return; }
    *pressed |= button;
    *held |= button;
    status(seq, "injected");
    printf("Nexo X4: dev command #%llu injected page=%d\n", (unsigned long long)seq, page);
}

void x4_dev_control_capture(X4DevControl *control, int page, const uint32_t *pixels)
{
    if (!control || !control->capture_pending) return;
    control->capture_pending = 0;
    uint64_t seq = control->capture_seq;
    if (page != 5 || !pixels || !enabled()) { status(seq, "refused-page"); return; }
    FILE *f = fopen(X4_DEV_DIR "screenshot.ppm.partial", "wb");
    unsigned char *row = malloc((size_t)X4_WIDTH * 3);
    if (!f || !row) {
        if (f) fclose(f);
        free(row);
        (void)remove(X4_DEV_DIR "screenshot.ppm.partial");
        status(seq, "capture-failed");
        return;
    }
    int ok = fprintf(f, "P6\n%d %d\n255\n", X4_WIDTH, X4_HEIGHT) > 0;
    for (int y = 0; ok && y < X4_HEIGHT; ++y) {
        for (int x = 0; x < X4_WIDTH; ++x) {
            uint32_t pixel = pixels[(size_t)y * X4_WIDTH + x];
            row[3 * x] = (unsigned char)pixel;
            row[3 * x + 1] = (unsigned char)(pixel >> 8);
            row[3 * x + 2] = (unsigned char)(pixel >> 16);
        }
        ok = fwrite(row, 1, (size_t)X4_WIDTH * 3, f) == (size_t)X4_WIDTH * 3;
    }
    free(row);
    ok = fflush(f) == 0 && ok;
    ok = fclose(f) == 0 && ok;
    if (ok) ok = rename(X4_DEV_DIR "screenshot.ppm.partial", X4_DEV_DIR "screenshot.ppm") == 0;
    if (!ok) (void)remove(X4_DEV_DIR "screenshot.ppm.partial");
    status(seq, ok ? "capture-ready" : "capture-failed");
    printf("Nexo X4: dev capture #%llu %s\n", (unsigned long long)seq, ok ? "ready" : "failed");
}
#endif
