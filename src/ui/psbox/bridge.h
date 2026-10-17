/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "../game_text.h"
#include "../../auth/xbox_catalog.h"
#include "../../auth/device_auth.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct X4PsboxUi X4PsboxUi;
enum X4PsboxButton {
    X4_PSB_UP = 1u << 0, X4_PSB_DOWN = 1u << 1,
    X4_PSB_LEFT = 1u << 2, X4_PSB_RIGHT = 1u << 3,
    X4_PSB_CROSS = 1u << 4, X4_PSB_CIRCLE = 1u << 5,
    X4_PSB_OPTIONS = 1u << 6, X4_PSB_L1 = 1u << 7,
    X4_PSB_R1 = 1u << 8, X4_PSB_SQUARE = 1u << 9,
    X4_PSB_TRIANGLE = 1u << 10, X4_PSB_R3 = 1u << 11,
    X4_PSB_L2 = 1u << 12, X4_PSB_R2 = 1u << 13,
    X4_PSB_TOUCHPAD = 1u << 14,
};
typedef struct {
    uint32_t pressed, held;
    float left_x, left_y, right_x, right_y, l2, r2;
    uint64_t now_ms;
} X4PsboxPad;
typedef struct {
    char region[80];
    int width, height, rtt_ms, degraded;
    double fps, mbps, loss_pct, decode_ms, on_screen_ms;
} X4PsboxStreamInfo; /* negative measurements mean unknown */
enum X4PsboxMenuAction { X4_PSB_MENU_NONE, X4_PSB_MENU_CLOSE,
    X4_PSB_MENU_GUIDE, X4_PSB_MENU_LEAVE, X4_PSB_MENU_STATS,
    X4_PSB_MENU_PROFILE, X4_PSB_MENU_DIAGNOSTICS, X4_PSB_MENU_PREFS_FAILED };

enum X4PsboxAction { X4_PSB_ACTION_NONE, X4_PSB_ACTION_SIGN_IN,
    X4_PSB_ACTION_REFRESH_CATALOG, X4_PSB_ACTION_PLAY,
    X4_PSB_ACTION_SIGN_OUT, X4_PSB_ACTION_CANCEL_SESSION,
    X4_PSB_ACTION_UNSUPPORTED_SETTING, X4_PSB_ACTION_PREFS_SAVE_FAILED };
typedef struct {
    enum X4PsboxAction action;
    unsigned catalog_index; /* valid only for PLAY, against last set_catalog */
    char title_id[96];      /* same service titleId, copied for the session hook */
} X4PsboxEvent;

/* All calls are on the main/UI thread. `font_dir` is the installed path to
 * bundled Inter fonts. Returns NULL if those assets could not be read. */
X4PsboxUi *x4_psbox_create(const char *font_dir);
void x4_psbox_destroy(X4PsboxUi *ui);
/* Set a dedicated absolute writable directory (for example
 * /data/xcloud4-ui) before first catalog display. No account tokens enter it. */
int x4_psbox_set_prefs_dir(X4PsboxUi *ui, const char *directory);
typedef struct {
    char product_id[32], name[128], publisher[128], description[512];
    char tile_url[384], hero_url[384];
} X4PsboxMetadata;
#define X4_PSB_CURATED_MAX_ROWS 8u
#define X4_PSB_CURATED_MAX_ITEMS 512u
typedef struct {
    char name[128];
    size_t count;
    char product_ids[X4_PSB_CURATED_MAX_ITEMS][32];
    unsigned source_count; /* original service list count before intersection */
    int truncated; /* service list exceeded the fixed local bound */
} X4PsboxCuratedRow;
/* Copies only public snapshot fields, including the stable titleId mapping.
 * Call after x4_auth_catalog_snapshot_cached returns a new revision. */
int x4_psbox_set_catalog(X4PsboxUi *ui, const X4CatalogSnapshot *catalog);
/* Metadata is optional, matched by exact productId from real catalog entries;
 * no entry can add or change a service titleId or entitlement. */
int x4_psbox_set_metadata(X4PsboxUi *ui, const X4PsboxMetadata *metadata, size_t count);
/* Ordered public service lists; each ProductId is intersected with the
 * verified READY catalog. The full verified catalog remains in All Games. */
int x4_psbox_set_rows(X4PsboxUi *ui, const X4PsboxCuratedRow *rows, size_t count);
/* Returns 1 and a catalog-verified productId when the open Details screen
 * needs hydration; 0 when nothing is pending, -1 for invalid arguments. */
int x4_psbox_detail_wanted(X4PsboxUi *ui, char product_id[32], uint64_t now_ms);
void x4_psbox_set_auth(X4PsboxUi *ui, const X4AuthSnapshot *auth);
void x4_psbox_set_launch(X4PsboxUi *ui, const char *title_id, const char *status);
void x4_psbox_set_session(X4PsboxUi *ui, const X4SessionSnapshot *session, const char *title_id);
/* The returned event is a request for the caller; no network/session call is
 * made by this bridge. Supported PS4 settings are persisted; profile changes apply next session. */
X4PsboxEvent x4_psbox_handle(X4PsboxUi *ui, const X4PsboxPad *pad);
/* 1920x1080 RGBA bytes in the same 0xAABBGGRR layout as X4Display. */
int x4_psbox_render(X4PsboxUi *ui, uint32_t *pixels, size_t pixel_count, uint64_t now_ms);
/* Menu is composed over the actual decoded live frame; unsupported PS5
 * controls are absent. Caller consumes its action and neutralizes Xbox pad
 * input while the menu is open or being toggled. */
int x4_psbox_stream_profile(const X4PsboxUi *ui);
int x4_psbox_diagnostics(const X4PsboxUi *ui);
int x4_psbox_language(const X4PsboxUi *ui);
int x4_psbox_settings_open(const X4PsboxUi *ui);
void x4_psbox_show_closing(X4PsboxUi *ui);
int x4_psbox_menu_open(const X4PsboxUi *ui);
/* Whether menu, statistics or connection warning will read/blend live pixels. */
int x4_psbox_needs_overlay(const X4PsboxUi *ui, int degraded);
void x4_psbox_menu_toggle(X4PsboxUi *ui);
enum X4PsboxMenuAction x4_psbox_menu_handle(X4PsboxUi *ui, const X4PsboxPad *pad);
int x4_psbox_menu_composite(X4PsboxUi *ui, uint32_t *pixels, size_t pixel_count,
    const X4PsboxStreamInfo *info);
/* Read-only navigation diagnostics for host fixtures and integration logs. */
int x4_psbox_screen(const X4PsboxUi *ui);
int x4_psbox_tab(const X4PsboxUi *ui);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
extern "C" {
#endif
void x4_psbox_game_text_clear(X4PsboxUi *ui);
int x4_psbox_game_text_composite(X4PsboxUi *ui, const X4GameText *text,
    uint32_t *pixels, size_t pixel_count, unsigned revision, int pending);
#ifdef __cplusplus
}
#endif
