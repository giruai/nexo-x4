/* SPDX-License-Identifier: GPL-3.0-only */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include <orbis/libkernel.h>
#include "../video/display.h"
#include "../input/controller.h"
#include "../input/diagnostics.h"
#include "../ui/screen.h"
#include "../auth/auth_profile.h"
#include "../media/live_media.h"
#include "lifecycle.h"
#include "build_identity.h"
#include "bitrate_control.h"
#if X4_PSBOX_UI
#include "stream_input_guard.h"
#include "auto_restore_policy.h"
#include "../ui/psbox/bridge.h"
#include "../ui/psbox_backend/metadata.h"
#include "../media/live_scale.h"
#endif
#if X4_DEV_CONTROL
#include "dev_control.h"
#endif

static void trace_inactive(X4LiveMedia *live)
{
    if (live) {
        X4Trace *trace = x4_live_media_trace(live);
        x4_trace_monitor_epoch(trace, X4_MON_ACTOR_SESSION, X4_MON_EPOCH_TRANSITION);
        x4_trace_set_active(trace, false);
    }
}
static bool trace_session_active(enum X4SessionState state)
{
    return state != X4_SESSION_STOPPING && state != X4_SESSION_CLOSED &&
        state != X4_SESSION_CANCELLED && state != X4_SESSION_ERROR;
}
#if X4_PSBOX_UI
static uint32_t psbox_buttons(uint32_t mask)
{
    uint32_t out = 0;
#define X4_MAP_PAD(orbis, psbox) if (mask & ORBIS_PAD_BUTTON_##orbis) out |= X4_PSB_##psbox
    X4_MAP_PAD(UP, UP); X4_MAP_PAD(DOWN, DOWN);
    X4_MAP_PAD(LEFT, LEFT); X4_MAP_PAD(RIGHT, RIGHT);
    X4_MAP_PAD(CROSS, CROSS); X4_MAP_PAD(CIRCLE, CIRCLE);
    X4_MAP_PAD(OPTIONS, OPTIONS); X4_MAP_PAD(L1, L1);
    X4_MAP_PAD(R1, R1); X4_MAP_PAD(SQUARE, SQUARE);
    X4_MAP_PAD(TRIANGLE, TRIANGLE); X4_MAP_PAD(R3, R3);
    X4_MAP_PAD(L2, L2); X4_MAP_PAD(R2, R2);
    X4_MAP_PAD(TOUCH_PAD, TOUCHPAD);
#undef X4_MAP_PAD
    return out;
}
static X4PsboxPad psbox_pad(const X4Controller *controller, uint64_t now_us)
{
    X4PsboxPad pad = {0};
    pad.pressed = psbox_buttons(controller->pressed);
    pad.held = psbox_buttons(controller->data.buttons);
    if (controller->data.connected) {
        pad.left_x = ((float)controller->data.leftStick.x - 127.5f) / 127.5f;
        pad.left_y = ((float)controller->data.leftStick.y - 127.5f) / 127.5f;
        pad.right_x = ((float)controller->data.rightStick.x - 127.5f) / 127.5f;
        pad.right_y = ((float)controller->data.rightStick.y - 127.5f) / 127.5f;
        pad.l2 = (float)controller->data.analogButtons.l2 / 255.0f;
        pad.r2 = (float)controller->data.analogButtons.r2 / 255.0f;
    }
    pad.now_ms = now_us / 1000;
    return pad;
}

static bool psbox_restore_retryable(const X4AuthSnapshot *account)
{
    if (account->state == X4_AUTH_EXPIRED) return account->http_status == 0;
    if (account->state != X4_AUTH_ERROR) return false;
    if (account->http_status)
        return account->http_status == 408 || account->http_status == 429 ||
            account->http_status >= 500;
    /* Local validation, account refusal, and storage failures cannot be
     * repaired by repeatedly sending the same refresh credential. */
    return account->error != 0 &&
        !(account->error <= X4_AUTH_E_ARGUMENT && account->error >= X4_AUTH_E_STORE);
}
#endif

int main(void)
{
    /* Auth restoration can start a logging worker during x4_auth_create. */
    setvbuf(stdout, NULL, _IONBF, 0);
    X4Display display;
    X4Controller controller;
    X4Screen screen = {0};
    X4DemoVideo video = {0};
    X4DemoAudio audio = {.handle = -1};
    X4Auth *auth = x4_auth_create();
    X4AuthSnapshot account = {0};
    static X4CatalogSnapshot catalog;
    static X4CatalogBrowser catalog_browser;
    X4AuthCatalogCursor catalog_cursor = {0};
    X4SessionSnapshot session = {0};
    unsigned catalog_selected = 0;
    unsigned request_play_index = 0;
    int request_play = 0;
    int session_back = 0;
    int game_controls = 0;
    int media_active = 0;
    int closing = 0;
    X4LiveMedia *live = NULL;
    X4LiveMediaSnapshot live_status = {0};
    int live_error = 0, live_muted = 0;
    int live_retained = 0;
    bool live_presented = false;
    bool diagnostic_stable = false;
    uint64_t diagnostic_mark = 0;
    int overlay_key[11] = {0};
    uint64_t overlay_at = 0;
    uint64_t input_previous = 0, input_report_at = 0;
    uint64_t input_intervals = 0, input_interval_us = 0, input_interval_max_us = 0;
    uint64_t idle_present_skips = 0;
    X4BitrateControl bitrate_control;
    x4_bitrate_init(&bitrate_control, sceKernelGetProcessTime());
#if X4_PSBOX_UI
    X4PsboxUi *psbox = NULL;
    X4PsboxBackend *psbox_backend = NULL;
    X4AuthSnapshot psbox_auth_sent = {0};
    X4SessionSnapshot psbox_session_sent = {0};
    int psbox_auth_valid = 0, psbox_session_valid = 0;
    int psbox_catalog_requested = 0, psbox_forget_pending = 0;
    int psbox_auto_restore_paused = 0;
    int psbox_waiting_video = 0;
    uint64_t psbox_guide_until_us = 0;
    X4StreamInputGuard psbox_menu_guard = {0};
    X4AutoRestorePolicy psbox_auto_restore = {0};
    X4LiveMediaSnapshot psbox_stats_previous = {0};
    uint64_t psbox_stats_at = 0;
    X4PsboxStreamInfo psbox_stream_info = {.rtt_ms = -1, .mbps = -1, .loss_pct = -1};
    static X4PsboxMetadata psbox_metadata[X4_PSB_METADATA_LIGHT_BATCH];
    static X4PsboxCuratedRow psbox_rows[X4_PSB_CURATED_MAX_ROWS];
#endif
#if X4_DEV_CONTROL
    X4DevControl dev_control = {0};
#endif
#if X4_RUMBLE
    OrbisPadVibeParam rumble_applied={0};
    int rumble_handle=-1;
    bool rumble_was_connected=false;
    uint64_t rumble_pad_calls=0,rumble_pad_ok=0,rumble_pad_errors=0;
#endif
    x4_catalog_browser_init(&catalog_browser);
reopen_interface:;
    int rc = x4_display_open(&display);
    if (rc < 0) {
        printf("Nexo X4: no se pudo iniciar VideoOut: 0x%08x\n", (unsigned)rc);
        sceKernelUsleep(3000000);
        /* Never enter the known failing libc _exit path on this firmware. */
        if (x4_exit_prepare() >= 0) x4_exit_request();
        for (;;) sceKernelUsleep(100000);
    }
    x4_controller_init(&controller);
#if X4_PSBOX_UI
    psbox = x4_psbox_create("/app0/assets/psbox/fonts");
    if (psbox) {
        if (x4_psbox_set_prefs_dir(psbox, "/data/xcloud4-ui") < 0)
            printf("Nexo X4: preferencias PSBox no disponibles\n");
        psbox_backend = x4_psbox_backend_create();
        screen.page = 5;
        psbox_auth_valid = psbox_session_valid = 0;
        psbox_catalog_requested = psbox_forget_pending = 0;
        psbox_auto_restore_paused = 0;
        psbox_waiting_video = 0;
        psbox_guide_until_us = 0;
        psbox_menu_guard = (X4StreamInputGuard){0};
        x4_auto_restore_init(&psbox_auto_restore);
        psbox_stats_at = 0;
        psbox_stats_previous = (X4LiveMediaSnapshot){0};
        psbox_stream_info = (X4PsboxStreamInfo){.rtt_ms = -1, .mbps = -1, .loss_pct = -1};
        printf("Nexo X4: interfaz PSBox activa\n");
    } else {
        printf("Nexo X4: fuente PSBox no disponible; interfaz anterior\n");
    }
#endif
#if X4_RUMBLE
    rumble_applied=(OrbisPadVibeParam){0}; rumble_handle=-1; rumble_was_connected=false;
#endif
#ifdef X4_INGRESS_BASELINE
    printf("Nexo X4 %s BASE: solo para referencia, no es la prueba autorizada\n",X4_PRODUCT_VERSION);
#else
    printf("Nexo X4 %s: progreso independiente, base MPSC\n", X4_PRODUCT_VERSION);
#endif
    printf("Nexo X4: %s\n", X4_AUTH_PROFILE_NOTE);
    printf("Nexo X4: diagnostic build=%s\n", X4_BUILD_ID);
    for (unsigned frame = 0;; ++frame) {
#if X4_PSBOX_UI
        x4_input_diagnostics_set(x4_psbox_diagnostics(psbox) != 0);
#endif
        uint64_t input_at = sceKernelGetProcessTime();
        x4_trace_monitor_state(live ? x4_live_media_trace(live) : NULL,
            X4_MON_ACTOR_MAIN, X4_MON_PHASE_MAIN_POLL, input_at);
        x4_controller_read(&controller, frame);
#if X4_DEV_CONTROL
        int dev_page = closing ? -1 : screen.page;
#if X4_PSBOX_UI
        if (psbox && dev_page == 5) dev_page = 0x105;
#endif
        x4_dev_control_poll(&dev_control, input_at, dev_page,
            x4_auth_busy(auth),
            &controller.pressed, &controller.data.buttons);
#endif
        /* Local pad sampling cadence, not Xbox RTT or end-to-end latency. */
        if (screen.page == 6 && x4_auth_busy(auth)) {
            if (input_previous) {
                uint64_t elapsed = input_at - input_previous;
                ++input_intervals;
                input_interval_us += elapsed;
                if (elapsed > input_interval_max_us) input_interval_max_us = elapsed;
            }
            input_previous = input_at;
            if (!input_report_at) input_report_at = input_at;
            if (input_at - input_report_at >= 5000000) {
                if (x4_input_diagnostics_enabled()) printf("Nexo X4 input: samples=%llu mean_us=%llu max_us=%llu idle_flips_skipped=%llu\n",
                    (unsigned long long)input_intervals,
                    (unsigned long long)(input_intervals ? input_interval_us / input_intervals : 0),
                    (unsigned long long)input_interval_max_us,
                    (unsigned long long)idle_present_skips);
                input_report_at = input_at;
                input_intervals = input_interval_us = input_interval_max_us = idle_present_skips = 0;
            }
        } else {
            input_previous = input_report_at = 0;
            input_intervals = input_interval_us = input_interval_max_us = idle_present_skips = 0;
        }
        int previous_page = screen.page;
        /* Keep Xbox button ownership across transient RTC disconnects. A
         * momentary status change must never turn B/Menu into local exit. */
        if (!x4_auth_busy(auth) || screen.page != 6) game_controls = 0;
        else if (session.rtc_connected) game_controls = 1;
        bool game_input = screen.page == 6 && game_controls && x4_auth_busy(auth) &&
            !closing && !session_back && !live_error;
        bool chord = (controller.data.buttons & (ORBIS_PAD_BUTTON_L1 | ORBIS_PAD_BUTTON_R1)) ==
            (ORBIS_PAD_BUTTON_L1 | ORBIS_PAD_BUTTON_R1);
        bool local_action = game_input && chord && (controller.data.buttons &
            (ORBIS_PAD_BUTTON_OPTIONS | ORBIS_PAD_BUTTON_CIRCLE | ORBIS_PAD_BUTTON_SQUARE));
        bool guide_action = game_input && chord && (controller.data.buttons & ORBIS_PAD_BUTTON_TOUCH_PAD);
        bool diagnostic_action = game_input && chord &&
            (controller.data.buttons & ORBIS_PAD_BUTTON_TRIANGLE);
        bool psbox_menu_capture = false;
#if X4_PSBOX_UI
        enum X4PsboxMenuAction psbox_menu_action = X4_PSB_MENU_NONE;
        if (psbox && game_input) {
            bool was_open = x4_psbox_menu_open(psbox) != 0;
            bool toggle = live_status.video_ready && !chord &&
                (controller.pressed & ORBIS_PAD_BUTTON_OPTIONS);
            if (toggle) {
                x4_psbox_menu_toggle(psbox);
                if (was_open) x4_auth_request_keyframe(auth);
            } else if (was_open) {
                X4PsboxPad pad = psbox_pad(&controller, input_at);
                psbox_menu_action = x4_psbox_menu_handle(psbox, &pad);
            }
            psbox_menu_capture = was_open || toggle;
            if (psbox_menu_action == X4_PSB_MENU_CLOSE || psbox_menu_action == X4_PSB_MENU_GUIDE)
                x4_auth_request_keyframe(auth);
            if (psbox_menu_action == X4_PSB_MENU_PREFS_FAILED)
                printf("Nexo X4: no se pudieron guardar las opciones; siguen activas en memoria\n");
            if (psbox_menu_action == X4_PSB_MENU_GUIDE)
                psbox_guide_until_us = input_at + 250000;
        }
        if (psbox && screen.page == 6) {
            const OrbisPadData *pad = &controller.data;
            psbox_menu_capture = x4_stream_input_guard_step(&psbox_menu_guard,
                psbox_menu_capture, pad->buttons, pad->leftStick.x, pad->leftStick.y,
                pad->rightStick.x, pad->rightStick.y,
                pad->analogButtons.l2, pad->analogButtons.r2) != 0;
        } else psbox_menu_guard.waiting_neutral = 0;
#endif
        X4GamepadFrame gamepad = {0};
        if (game_input) x4_controller_gamepad(&controller, &gamepad);
        if (local_action || guide_action || diagnostic_action || psbox_menu_capture) {
            memset(&gamepad, 0, sizeof(gamepad));
            gamepad.connected = controller.data.connected;
            if (guide_action && !local_action && !diagnostic_action) gamepad.buttons = X4_GAMEPAD_NEXUS;
        }
#if X4_PSBOX_UI
        if (game_input && psbox_guide_until_us > input_at) {
            memset(&gamepad, 0, sizeof(gamepad));
            gamepad.buttons = X4_GAMEPAD_NEXUS;
            gamepad.connected = controller.data.connected;
        }
#endif
        x4_auth_set_gamepad(auth, &gamepad);
#if X4_PSBOX_UI
        if (psbox_menu_action == X4_PSB_MENU_LEAVE) {
            trace_inactive(live);
            x4_auth_cancel(auth);
            session_back = 1;
            x4_psbox_set_launch(psbox,
                catalog_selected < catalog.count ? catalog.titles[catalog_selected].id : NULL,
                "Cerrando juego");
        }
#endif
#if X4_RUMBLE
        X4RumbleOutput desired={0};
        /* A report can arrive after the frame-start input sample. Poll with
         * a fresh time so that earlier sample is not treated as clock reversal. */
        x4_auth_poll_rumble(auth,sceKernelGetProcessTime(),&desired);
        if((!controller.data.connected&&rumble_was_connected)||controller.handle<0) rumble_handle=-1;
        rumble_was_connected=controller.data.connected;
        if(!game_input||!session.rtc_connected||!controller.data.connected)
            desired.lgMotor=desired.smMotor=0;
        if(controller.handle>=0&&(rumble_handle!=controller.handle||
            rumble_applied.lgMotor!=desired.lgMotor||rumble_applied.smMotor!=desired.smMotor)) {
            OrbisPadVibeParam next={desired.lgMotor,desired.smMotor};
            int vibration_rc=scePadSetVibration(controller.handle,&next);
            ++rumble_pad_calls;
            if(vibration_rc>=0) ++rumble_pad_ok; else ++rumble_pad_errors;
            if(vibration_rc>=0) { rumble_applied=next; rumble_handle=controller.handle; }
        }
#endif
        if (!closing) {
            uint32_t ui_pressed = controller.pressed;
            if (
#if X4_PSBOX_UI
                x4_stream_options_owned(psbox != NULL, screen.page, game_input)
#else
                game_input
#endif
                ) ui_pressed &= ~ORBIS_PAD_BUTTON_OPTIONS;
            /* CIRCLE finishes legacy keyboard editing before account
             * navigation. PSBox page 6 owns plain OPTIONS, even while its
             * session is still connecting; the exit chord remains below. */
            if (screen.page == 5 && catalog_browser.editing
#if X4_PSBOX_UI
                && !psbox
#endif
                )
                ui_pressed &= ~ORBIS_PAD_BUTTON_CIRCLE;
            if (
#if X4_PSBOX_UI
                !(psbox && screen.page == 5)
#else
                true
#endif
                ) x4_screen_update(&screen, ui_pressed, controller.data.buttons);
#if X4_PSBOX_UI
            if (psbox && screen.page == 5 && chord &&
                (controller.pressed & ORBIS_PAD_BUTTON_OPTIONS))
                screen.exit_requested = 1;
#endif
            if (
#if X4_PSBOX_UI
                x4_stream_exit_chord(psbox != NULL, screen.page, game_input,
                    chord, (controller.pressed & ORBIS_PAD_BUTTON_OPTIONS) != 0)
#else
                game_input && chord && (controller.pressed & ORBIS_PAD_BUTTON_OPTIONS)
#endif
                )
                screen.exit_requested = 1;
        }
        if ((previous_page == 4 || previous_page == 5 || previous_page == 6) && screen.page != previous_page) {
            trace_inactive(live);
            x4_auth_cancel(auth);
        }
        if (screen.exit_requested) {
            trace_inactive(live);
            if (x4_auth_busy(auth)) {
                x4_auth_cancel(auth);
                closing = 1;
            } else {
                closing = 0;
                screen.exit_error = x4_exit_prepare();
                if (screen.exit_error < 0) {
                    printf("Nexo X4: preparar salida fallo 0x%08x\n", (unsigned)screen.exit_error);
                    screen.exit_requested = 0;
                } else break;
            }
        }
        if (!closing && screen.page == 4 && previous_page == 4) {
            if (controller.pressed & ORBIS_PAD_BUTTON_CROSS) x4_auth_start(auth, X4_AUTH_SIGN_IN);
            else if (controller.pressed & ORBIS_PAD_BUTTON_SQUARE) x4_auth_start(auth, X4_AUTH_CHECK_CONNECTION);
            else if (controller.pressed & ORBIS_PAD_BUTTON_TRIANGLE) x4_auth_forget(auth);
            else if ((controller.pressed & ORBIS_PAD_BUTTON_R1) &&
                     account.state == X4_AUTH_AUTHORIZED && !x4_auth_busy(auth)) {
                screen.page = 5;
                if (catalog.state != X4_CATALOG_READY) {
                    catalog_selected = 0;
                    x4_auth_start(auth, X4_AUTH_XBOX_CATALOG);
                }
            }
        }
        x4_trace_monitor_state(live ? x4_live_media_trace(live) : NULL,
            X4_MON_ACTOR_MAIN, X4_MON_PHASE_MAIN_STATUS, 0);
        x4_auth_snapshot(auth, &account);
        bool catalog_copied = x4_auth_catalog_snapshot_cached(auth, &catalog, &catalog_cursor) != 0;
        if (catalog_copied) {
            x4_catalog_browser_rebuild(&catalog_browser, &catalog);
            if (catalog.state != X4_CATALOG_READY) catalog_browser.editing = false;
        }
        x4_auth_session_snapshot(auth, &session);
#if X4_PSBOX_UI
        if (psbox) {
            if (!psbox_auth_valid || memcmp(&psbox_auth_sent, &account, sizeof(account))) {
                x4_psbox_set_auth(psbox, &account);
                psbox_auth_sent = account;
                psbox_auth_valid = 1;
                if (account.state != X4_AUTH_AUTHORIZED) psbox_catalog_requested = 0;
            }
            if (catalog_copied) {
                (void)x4_psbox_set_catalog(psbox, &catalog);
                if (catalog.state == X4_CATALOG_LOADING || catalog.state == X4_CATALOG_READY)
                    psbox_catalog_requested = 1;
                if (psbox_backend && catalog.state == X4_CATALOG_READY)
                    (void)x4_psbox_backend_set_catalog(psbox_backend, &catalog);
            }
            if (psbox_forget_pending && !x4_auth_busy(auth)) {
                int forget_rc = x4_auth_forget(auth);
                printf("Nexo X4: cerrar sesion local=%d\n", forget_rc);
                psbox_forget_pending = 0;
                psbox_catalog_requested = 0;
                x4_auto_restore_stop(&psbox_auto_restore);
            }
            bool restore_eligible = !closing && !psbox_forget_pending &&
                !psbox_auto_restore_paused &&
                screen.page == 5 && !session_back && !session.cleanup_failed &&
                (session.state == X4_SESSION_IDLE || session.state == X4_SESSION_CLOSED ||
                 session.state == X4_SESSION_CANCELLED || session.state == X4_SESSION_ERROR);
            if (x4_auto_restore_due(&psbox_auto_restore, input_at,
                account.state == X4_AUTH_AUTHORIZED,
                psbox_restore_retryable(&account), x4_auth_busy(auth), restore_eligible)) {
                int restore_rc = x4_auth_restore_saved_if_idle(auth);
                if (!restore_rc) x4_auto_restore_start(&psbox_auto_restore);
                else if (restore_rc == X4_AUTH_E_SIGNED_OUT)
                    x4_auto_restore_stop(&psbox_auto_restore);
                else if (restore_rc != X4_AUTH_E_BUSY && restore_rc != X4_AUTH_E_AUTHORIZED)
                    x4_auto_restore_delay(&psbox_auto_restore, input_at);
            }
            if (!closing && !psbox_forget_pending && account.state == X4_AUTH_AUTHORIZED &&
                catalog.state == X4_CATALOG_IDLE && !psbox_catalog_requested && !x4_auth_busy(auth)) {
                int catalog_rc = x4_auth_start(auth, X4_AUTH_XBOX_CATALOG);
                if (!catalog_rc) psbox_catalog_requested = 1;
                else printf("Nexo X4: inicio catalogo PSBox=%d\n", catalog_rc);
            }
            if (psbox_backend && catalog.state == X4_CATALOG_READY) {
                size_t row_count = 0;
                if (x4_psbox_backend_take_rows(psbox_backend, psbox_rows,
                    X4_PSB_CURATED_MAX_ROWS, &row_count) == 1)
                    (void)x4_psbox_set_rows(psbox, psbox_rows, row_count);
                size_t metadata_count = 0;
                if (x4_psbox_backend_take_metadata(psbox_backend, psbox_metadata,
                    X4_PSB_METADATA_LIGHT_BATCH, &metadata_count) == 1 && metadata_count)
                    (void)x4_psbox_set_metadata(psbox, psbox_metadata, metadata_count);
                char detail_product[32];
                if (x4_psbox_detail_wanted(psbox, detail_product, input_at / 1000) == 1)
                    (void)x4_psbox_backend_request_detail(psbox_backend, detail_product);
            }
            if (session.state != X4_SESSION_IDLE &&
                (!psbox_session_valid || memcmp(&psbox_session_sent, &session, sizeof(session)))) {
                const char *title = catalog_selected < catalog.count ? catalog.titles[catalog_selected].id : NULL;
                x4_psbox_set_session(psbox, &session, title);
                psbox_session_sent = session;
                psbox_session_valid = 1;
                if (session.state == X4_SESSION_STREAMING) psbox_waiting_video = 0;
            }
        }
#endif
        if (!closing && screen.page == 6 && previous_page == 6) {
#if X4_PSBOX_UI
            /* The error screen advertises X = Retry. Reuse the ordinary play
             * path only after the prior worker and remote cleanup finish. */
            if (psbox && (controller.pressed & ORBIS_PAD_BUTTON_CROSS) &&
                !(controller.pressed & ORBIS_PAD_BUTTON_CIRCLE) && !screen.exit_requested &&
                session.state == X4_SESSION_ERROR && !session.cleanup_failed &&
                !session_back && !live_error && !live_retained &&
                !x4_auth_busy(auth) && account.state == X4_AUTH_AUTHORIZED &&
                catalog.state == X4_CATALOG_READY && catalog_selected < catalog.count &&
                catalog.titles[catalog_selected].entitled &&
                catalog.titles[catalog_selected].id[0]) {
                request_play_index = catalog_selected;
                request_play = 1;
                printf("Nexo X4: reintento manual de sesion, indice=%u\n", catalog_selected);
            }
#endif
            if ((controller.pressed & ORBIS_PAD_BUTTON_CIRCLE) && (!game_input || chord) &&
                !psbox_menu_capture) {
                trace_inactive(live);
                x4_auth_cancel(auth);
                session_back = 1;
#if X4_PSBOX_UI
                if (psbox) x4_psbox_set_launch(psbox,
                    catalog_selected < catalog.count ? catalog.titles[catalog_selected].id : NULL,
                    "Cerrando juego");
#endif
            }
            /* Keep the session page visible until remote cleanup finishes. */
            if (session_back && !x4_auth_busy(auth)) {
                session_back = 0;
                screen.page = 5;
            }
        }
        if (!closing && screen.page == 5 && previous_page == 5) {
#if X4_PSBOX_UI
            if (psbox) {
                if (!(chord && (controller.pressed & ORBIS_PAD_BUTTON_OPTIONS))) {
                    X4PsboxPad pad = psbox_pad(&controller, input_at);
                    X4PsboxEvent event = x4_psbox_handle(psbox, &pad);
                    switch (event.action) {
                    case X4_PSB_ACTION_SIGN_IN:
                        psbox_auto_restore_paused = 0;
                        if (!x4_auth_busy(auth)) (void)x4_auth_start(auth, X4_AUTH_SIGN_IN);
                        break;
                    case X4_PSB_ACTION_REFRESH_CATALOG:
                        if (!x4_auth_busy(auth) && account.state == X4_AUTH_AUTHORIZED) {
                            if (!x4_auth_start(auth, X4_AUTH_XBOX_CATALOG)) psbox_catalog_requested = 1;
                        }
                        break;
                    case X4_PSB_ACTION_SIGN_OUT:
                        psbox_auto_restore_paused = 1;
                        psbox_forget_pending = 1;
                        x4_auth_cancel(auth);
                        break;
                    case X4_PSB_ACTION_CANCEL_SESSION:
                        trace_inactive(live);
                        x4_auth_cancel(auth);
                        session_back = 1;
                        break;
                    case X4_PSB_ACTION_PLAY:
                        if (!catalog_copied && !x4_auth_busy(auth) &&
                            catalog.state == X4_CATALOG_READY &&
                            event.catalog_index < catalog.count &&
                            catalog.titles[event.catalog_index].entitled &&
                            !strcmp(catalog.titles[event.catalog_index].id, event.title_id)) {
                            request_play_index = event.catalog_index;
                            request_play = 1;
                        }
                        break;
                    case X4_PSB_ACTION_UNSUPPORTED_SETTING:
                        printf("Nexo X4: ajuste PSBox no disponible\n");
                        break;
                    case X4_PSB_ACTION_PREFS_SAVE_FAILED:
                        printf("Nexo X4: preferencias PSBox no guardadas\n");
                        break;
                    default: break;
                    }
                }
            } else
#endif
            {
            uint32_t pressed = controller.pressed;
            if (catalog_browser.editing) {
                if (pressed & (ORBIS_PAD_BUTTON_R2 | ORBIS_PAD_BUTTON_CIRCLE)) {
                    catalog_browser.editing = false;
                } else {
                    if (pressed & ORBIS_PAD_BUTTON_UP)
                        x4_catalog_browser_keyboard_move(&catalog_browser, 0, -1);
                    if (pressed & ORBIS_PAD_BUTTON_DOWN)
                        x4_catalog_browser_keyboard_move(&catalog_browser, 0, 1);
                    if (pressed & ORBIS_PAD_BUTTON_LEFT)
                        x4_catalog_browser_keyboard_move(&catalog_browser, -1, 0);
                    if (pressed & ORBIS_PAD_BUTTON_RIGHT)
                        x4_catalog_browser_keyboard_move(&catalog_browser, 1, 0);
                    if (pressed & ORBIS_PAD_BUTTON_TRIANGLE)
                        x4_catalog_browser_query_clear(&catalog_browser);
                    else if (pressed & ORBIS_PAD_BUTTON_SQUARE)
                        x4_catalog_browser_query_delete(&catalog_browser);
                    else if (pressed & ORBIS_PAD_BUTTON_R1)
                        x4_catalog_browser_query_append(&catalog_browser, ' ');
                    else if (pressed & ORBIS_PAD_BUTTON_CROSS)
                        x4_catalog_browser_query_append(&catalog_browser,
                            x4_catalog_browser_keyboard_char(&catalog_browser));
                }
                if (catalog_browser.criteria_dirty)
                    x4_catalog_browser_rebuild(&catalog_browser, &catalog);
            } else if ((pressed & ORBIS_PAD_BUTTON_TRIANGLE) &&
                       catalog.state == X4_CATALOG_READY && !x4_auth_busy(auth)) {
                catalog_browser.editing = true;
            } else if ((pressed & ORBIS_PAD_BUTTON_SQUARE) && !x4_auth_busy(auth)) {
                x4_auth_start(auth, X4_AUTH_XBOX_CATALOG);
            } else {
                if (pressed & ORBIS_PAD_BUTTON_L2)
                    x4_catalog_browser_cycle_filter(&catalog_browser, -1);
                if (pressed & ORBIS_PAD_BUTTON_R2)
                    x4_catalog_browser_cycle_filter(&catalog_browser, 1);
                if (catalog_browser.criteria_dirty)
                    x4_catalog_browser_rebuild(&catalog_browser, &catalog);
                if (pressed & ORBIS_PAD_BUTTON_UP)
                    x4_catalog_browser_move(&catalog_browser, &catalog, -1, false);
                if (pressed & ORBIS_PAD_BUTTON_DOWN)
                    x4_catalog_browser_move(&catalog_browser, &catalog, 1, false);
                if (pressed & ORBIS_PAD_BUTTON_L1)
                    x4_catalog_browser_move(&catalog_browser, &catalog, -8, true);
                if (pressed & ORBIS_PAD_BUTTON_R1)
                    x4_catalog_browser_move(&catalog_browser, &catalog, 8, true);
                unsigned source_index;
                /* A fresh response or a navigation/filter press can select a
                 * row that has not been shown yet. Require a later X edge. */
                bool selection_changing = (pressed & (ORBIS_PAD_BUTTON_UP | ORBIS_PAD_BUTTON_DOWN |
                    ORBIS_PAD_BUTTON_L1 | ORBIS_PAD_BUTTON_R1 | ORBIS_PAD_BUTTON_L2 | ORBIS_PAD_BUTTON_R2)) != 0;
                if ((pressed & ORBIS_PAD_BUTTON_CROSS) && !catalog_copied && !selection_changing &&
                    !x4_auth_busy(auth) &&
                    x4_catalog_browser_selected_index(&catalog_browser, &catalog, &source_index)) {
                    /* The visible filtered ordinal is never a session index. */
                    request_play_index = source_index;
                    request_play = 1;
                }
            }
            }
        }
        if (request_play && !closing && !x4_auth_busy(auth)) {
                    request_play = 0;
                    catalog_selected = request_play_index;
                    live_presented = false;
                    live_error = 0;
#if X4_PSBOX_UI
                    psbox_stats_at = 0;
                    psbox_stats_previous = (X4LiveMediaSnapshot){0};
                    psbox_stream_info = (X4PsboxStreamInfo){.rtt_ms = -1, .mbps = -1, .loss_pct = -1};
#endif
                    if (live) {
                        trace_inactive(live);
                        live_error = x4_auth_set_media_callback(auth, NULL, NULL, NULL);
                        if (!live_error) live_error = x4_live_media_close(live);
                        if (!live_error) live = NULL;
                    }
                    if (!live_error) {
                        live_retained = 0;
                        int selected_profile = 0;
#if X4_PSBOX_UI
                        selected_profile = x4_psbox_stream_profile(psbox);
#endif
                        live_error = x4_auth_set_stream_profile(auth, selected_profile);
                        if (!live_error) live = x4_live_media_create_profile(selected_profile, &live_error);
                        x4_bitrate_init_profile(&bitrate_control, input_at, selected_profile);
                        diagnostic_stable = false;
                        diagnostic_mark = 0;
                        if (live) {
                            live_error = x4_live_media_set_payload_type(live, X4_LIVE_KIND_VIDEO, 102);
                            if (!live_error) live_error = x4_live_media_set_payload_type(live, X4_LIVE_KIND_AUDIO, 111);
                            if (!live_error) live_error = x4_live_media_start(live);
                        }
                    }
                    memset(&live_status, 0, sizeof(live_status));
                    live_muted = 0;
                    rc = live_error;
                    if (!rc) rc = x4_auth_set_media_callback(auth, x4_live_media_receive, live, x4_live_media_trace(live));
                    if (!rc) rc = x4_auth_set_grace_inbox(auth,x4_live_media_grace_inbox(live));
                    if (!rc) { game_controls = 0; rc = x4_auth_start_session(auth, catalog_selected); }
                    if (rc && !live_error) live_error = rc;
                    x4_auth_session_snapshot(auth, &session);
                    if (rc == 0 || session.state == X4_SESSION_ERROR || live_error) {
                        session_back = 0;
                        screen.page = 6;
                    }
                    if (rc == X4_AUTH_E_SIGNED_OUT) {
#if X4_PSBOX_UI
                        if (!psbox)
#endif
                            screen.page = 4;
                    }
                    if (rc) printf("Nexo X4: solicitar sesion fallo 0x%08x\n", (unsigned)rc);
        }
        if (live && !live_retained) {
            x4_live_media_tick(live);
            x4_live_media_snapshot(live, &live_status);
#if X4_DEV_CONTROL
            if (dev_control.abr_action) {
                unsigned action = dev_control.abr_action;
                dev_control.abr_action = 0;
                if (screen.page == 6 && session.state == X4_SESSION_STREAMING) {
                    if (action == 4) (void)x4_live_media_probe_loss(live, input_at);
                    else x4_bitrate_probe(&bitrate_control, input_at,
                        action == 1 ? 4000u : action == 2 ? 8000u : 0u);
                    printf("Nexo X4: ABR diagnostic_action=%u\n", action);
                }
            }
#endif
            x4_bitrate_tick(&bitrate_control, input_at, auth, &session, &live_status);
#if X4_PSBOX_UI
            if (psbox && screen.page == 6) {
                psbox_stream_info.width = (int)live_status.width;
                psbox_stream_info.height = (int)live_status.height;
                psbox_stream_info.mbps = bitrate_control.receive_kbps ?
                    bitrate_control.receive_kbps / 1000.0 : -1;
                snprintf(psbox_stream_info.region, sizeof(psbox_stream_info.region), "%s", session.region);
                if (psbox_stats_at && input_at - psbox_stats_at >= 1000000) {
                    double seconds = (double)(input_at - psbox_stats_at) / 1000000.0;
                    uint64_t frames = live_status.video_present_new - psbox_stats_previous.video_present_new;
                    uint64_t packets = live_status.video_packets - psbox_stats_previous.video_packets;
                    uint64_t lost = live_status.video_lost_packets - psbox_stats_previous.video_lost_packets;
                    uint64_t decodes = live_status.video_decode_calls - psbox_stats_previous.video_decode_calls;
                    uint64_t decode_us = live_status.video_decode_us - psbox_stats_previous.video_decode_us;
                    psbox_stream_info.fps = frames / seconds;
                    psbox_stream_info.loss_pct = packets + lost ? 100.0 * lost / (packets + lost) : -1;
                    psbox_stream_info.decode_ms = decodes ? (double)decode_us / decodes / 1000.0 : 0;
                    psbox_stream_info.degraded = psbox_stream_info.loss_pct > 5.0;
                    psbox_stats_previous = live_status;
                    psbox_stats_at = input_at;
                } else if (!psbox_stats_at) {
                    psbox_stats_previous = live_status;
                    psbox_stats_at = input_at;
                }
            }
#endif
            if (x4_auth_busy(auth)) {
                if (x4_live_media_take_keyframe_request(live)) x4_auth_request_keyframe(auth);
                if (live_status.video_error && !session_back) {
                    printf("Nexo X4: error de video nativo 0x%08x; cerrando sesion\n",
                        (unsigned)live_status.video_error);
                    trace_inactive(live);
                    x4_auth_cancel(auth);
                    session_back = 1;
#if X4_PSBOX_UI
                    if (psbox) x4_psbox_set_launch(psbox,
                        catalog_selected < catalog.count ? catalog.titles[catalog_selected].id : NULL,
                        "Cerrando juego tras error de video");
#endif
                }
                if (screen.page == 6 && (controller.pressed & ORBIS_PAD_BUTTON_SQUARE) && (!game_input || chord)) {
                    live_muted = !live_muted;
                    x4_live_media_set_muted(live, live_muted != 0);
                }
            } else {
                /* The worker closes RTC before publishing completion. */
                trace_inactive(live);
                int stop_rc = x4_auth_set_media_callback(auth, NULL, NULL, NULL);
                if (!stop_rc) stop_rc = x4_live_media_close(live);
                if (!stop_rc) live = NULL;
                else { live_error = stop_rc; live_retained = 1; printf("Nexo X4: medios retienen recursos, cierre 0x%08x\n", (unsigned)stop_rc); }
            }
        }
        if (screen.page != 3 && media_active) {
            x4_audio_stop(&audio);
            x4_video_stop(&video);
            media_active = 0;
        }
        if (screen.page == 3) {
            if (!media_active || (controller.pressed & ORBIS_PAD_BUTTON_CROSS)) {
                /* Present the menu before entering potentially slow system APIs. */
                x4_screen_draw(&screen, &controller, x4_display_pixels(&display));
                x4_media_draw(&video, &audio, x4_display_pixels(&display));
                rc = x4_display_present(&display);
                if (rc < 0) break;
                x4_audio_stop(&audio);
                x4_video_start(&video);
                x4_audio_start(&audio);
                media_active = 1;
            }
            if (controller.pressed & ORBIS_PAD_BUTTON_SQUARE)
                atomic_store(&audio.muted, !atomic_load(&audio.muted));
            x4_video_tick(&video);
        }
        /* Keep scanout unchanged between completed pictures. A real UI
         * transition or periodic status refresh still redraws the entire
         * inactive framebuffer, so two-buffer reuse never mixes old pixels. */
        bool live_visible = screen.page == 6 && live && x4_auth_busy(auth) && !closing && !session_back &&
            !live_error && !live_status.video_error && session.state != X4_SESSION_STOPPING &&
            live_status.video_ready;
#if X4_PSBOX_UI
        if (psbox && screen.page == 6 && session.state == X4_SESSION_STREAMING &&
            !live_visible && !live_error && !psbox_waiting_video) {
            const char *title = catalog_selected < catalog.count ? catalog.titles[catalog_selected].id : NULL;
            x4_psbox_set_launch(psbox, title, "Esperando imagen del juego");
            psbox_waiting_video = 1;
        }
        if (live_visible) psbox_waiting_video = 0;
#endif
        X4Trace *live_trace = live ? x4_live_media_trace(live) : NULL;
        if (live_visible && diagnostic_action && (controller.pressed & ORBIS_PAD_BUTTON_TRIANGLE) &&
            !local_action && !guide_action) {
            diagnostic_stable = !diagnostic_stable;
            unsigned epoch = diagnostic_stable ? X4_MON_EPOCH_STABLE : X4_MON_EPOCH_TRANSITION;
            x4_trace_monitor_epoch(live_trace, X4_MON_ACTOR_SESSION, epoch);
            x4_trace_record(live_trace, X4_TRACE_SESSION_PHASE, 0, epoch, ++diagnostic_mark);
            printf("Nexo X4: diagnostic phase=%s mark=%llu\n",
                diagnostic_stable ? "stable" : "transition", (unsigned long long)diagnostic_mark);
        }
        bool trace_active = live_visible && !live_retained && trace_session_active(session.state) &&
            x4_input_diagnostics_enabled();
        x4_trace_set_active(live_trace, trace_active);
        /* Runs before the idle continue too: a repeated/status-only refresh
         * must not conceal a gap in actual NEW live completions. */
        x4_trace_poll(live_trace);
        int next_overlay_key[11] = {screen.page, live_muted, live_status.audio_error,
            live_status.audio_playing, session.input_error, session.input_ready,
            session.rtc_connected, session.state, screen.exit_error, controller.data.connected, chord};
        uint64_t draw_at = sceKernelGetProcessTime();
        bool overlay_changed = memcmp(overlay_key, next_overlay_key, sizeof(overlay_key)) != 0;
        if (live_visible && live_presented && !overlay_changed && draw_at - overlay_at < 500000 &&
            !x4_live_media_has_new_picture(live)) {
            ++idle_present_skips;
            x4_trace_monitor_state(live_trace, X4_MON_ACTOR_MAIN, X4_MON_PHASE_MAIN_IDLE, 0);
            sceKernelUsleep(2000);
            continue;
        }
        x4_trace_monitor_state(live_trace, X4_MON_ACTOR_MAIN, X4_MON_PHASE_DRAW, 0);
        uint32_t *display_pixels = x4_display_pixels(&display);
        uint32_t *live_pixels = display_pixels;
        /* Opaque overlay copies do not read VideoOut: no full-frame scratch pass. */
        bool drew_live = live_visible && x4_live_media_draw(live, live_pixels);
        uint64_t drawn_generation = 0, drawn_output = 0;
        bool drawn_fresh = false;
        if (drew_live && !live_retained)
            x4_live_media_drawn(live, &drawn_generation, &drawn_output, &drawn_fresh);
        /* Media draw records the independent native-output identity. Display
         * uses the actual drawn publication tag, never a later snapshot. */
        (void)drawn_output;
        if (!drew_live) {
#if X4_PSBOX_UI
            if (!(psbox && !closing && (screen.page == 5 || screen.page == 6)))
#endif
                x4_screen_draw(&screen, &controller, x4_display_pixels(&display));
        }
        if (screen.page == 3) x4_media_draw(&video, &audio, x4_display_pixels(&display));
        if (screen.page == 6) {
            if (drew_live) {
#if X4_PSBOX_UI
                if (psbox) {
                    x4_live_overlay(&live_status, &session, live_muted, 0, live_pixels);
                    (void)x4_psbox_menu_composite(psbox, live_pixels,
                        (size_t)X4_WIDTH * X4_HEIGHT, &psbox_stream_info);
                } else
#endif
                    x4_live_overlay(&live_status, &session, live_muted, chord, live_pixels);
            }
            else {
#if X4_PSBOX_UI
                if (psbox && !closing) {
                    if (live_error && !x4_auth_busy(auth)) {
                        X4SessionSnapshot shown = session;
                        shown.state = X4_SESSION_ERROR;
                        snprintf(shown.stage, sizeof(shown.stage),
                            "no se pudieron preparar o cerrar los medios nativos");
                        x4_psbox_set_session(psbox, &shown,
                            catalog_selected < catalog.count ? catalog.titles[catalog_selected].id : NULL);
                    }
                    (void)x4_psbox_render(psbox, x4_display_pixels(&display),
                        (size_t)X4_WIDTH * X4_HEIGHT, draw_at / 1000);
                } else
#endif
                {
                X4SessionSnapshot shown = session;
                if (live_error && !x4_auth_busy(auth)) {
                    shown.state = X4_SESSION_ERROR;
                    shown.error = live_error;
                    shown.http_status = 0;
                    shown.ready_seen = shown.connection_authorized = 0;
                    shown.rtc_connected = 0;
                    snprintf(shown.stage, sizeof(shown.stage), "no se pudieron preparar o cerrar los medios nativos");
                    if (catalog_selected < catalog.count)
                        snprintf(shown.title_name, sizeof(shown.title_name), "%s", catalog.titles[catalog_selected].name);
                }
                x4_session_draw(&shown, x4_auth_busy(auth), closing, x4_display_pixels(&display));
                x4_live_status_draw(&live_status, live_error, x4_display_pixels(&display));
                }
            }
        }
        else if (screen.page == 4 || closing)
            x4_auth_draw(&account, x4_auth_busy(auth), closing, x4_display_pixels(&display));
        else if (screen.page == 5) {
#if X4_PSBOX_UI
            if (psbox && !closing)
                (void)x4_psbox_render(psbox, x4_display_pixels(&display),
                    (size_t)X4_WIDTH * X4_HEIGHT, draw_at / 1000);
            else
#endif
                x4_catalog_draw(&catalog, &catalog_browser, x4_auth_busy(auth), x4_display_pixels(&display));
        }
        x4_exit_error_draw(screen.exit_error, drew_live ? live_pixels : display_pixels);
#if X4_DEV_CONTROL
        x4_dev_control_capture(&dev_control, closing ? -1 : screen.page,
            x4_display_pixels(&display));
#endif
        uint64_t present_begin = sceKernelGetProcessTime();
        static uint64_t render_report_at, render_total_us, render_max_us, render_frames;
        static int render_previous_menu = -1;
        if (!x4_input_diagnostics_enabled()) {
            render_report_at = render_total_us = render_max_us = render_frames = 0;
            render_previous_menu = -1;
        } else if (drew_live) {
            int menu = 0;
#if X4_PSBOX_UI
            menu = x4_psbox_menu_open(psbox);
#endif
            if (menu != render_previous_menu) { render_total_us = render_max_us = render_frames = 0; render_report_at = present_begin; render_previous_menu = menu; }
            uint64_t elapsed = present_begin - draw_at;
            render_total_us += elapsed; ++render_frames; if (elapsed > render_max_us) render_max_us = elapsed;
            if (present_begin - render_report_at >= 5000000) {
                printf("Nexo render: menu=%d render_frames=%llu mean_us=%llu render_max_us=%llu width=%u height=%u\n",
                    menu, (unsigned long long)render_frames, (unsigned long long)(render_total_us / render_frames),
                    (unsigned long long)render_max_us, live_status.width, live_status.height);
                render_report_at = present_begin; render_total_us = render_max_us = render_frames = 0;
            }
        }
        rc = drew_live && trace_active ?
            x4_display_present_trace(&display, live_trace, drawn_generation, drawn_fresh) :
            x4_display_present(&display);
        if (rc == 0 && drew_live && live && !live_retained) {
            x4_live_media_note_present(live, sceKernelGetProcessTime() - present_begin);
            memcpy(overlay_key, next_overlay_key, sizeof(overlay_key));
            overlay_at = draw_at;
            live_presented = true;
        } else live_presented = false;
        if (rc < 0) {
            printf("Nexo X4: error de presentacion: 0x%08x\n", (unsigned)rc);
            break;
        }
    }
    /* A display failure can break the loop early: cancel then join before
     * cleaning the UI. Ordinary OPTIONS keeps rendering while it cancels. */
    trace_inactive(live);
    x4_auth_cancel(auth);
#if X4_RUMBLE
    if(controller.handle>=0) {
        OrbisPadVibeParam zero={0};
        (void)scePadSetVibration(controller.handle,&zero);
        rumble_applied=zero; rumble_handle=controller.handle;
    }
    printf("Nexo X4: vibracion Pad calls=%llu success=%llu errors=%llu (resultado local)\n",
        (unsigned long long)rumble_pad_calls,(unsigned long long)rumble_pad_ok,
        (unsigned long long)rumble_pad_errors);
#endif
    while (x4_auth_busy(auth)) sceKernelUsleep(100000);
    if (live) {
        int live_rc = x4_auth_set_media_callback(auth, NULL, NULL, NULL);
        if (!live_rc) live_rc = x4_live_media_close(live);
        if (!live_rc) live = NULL;
        else live_error = live_rc;
    }
#if X4_PSBOX_UI
    if (psbox_backend) { x4_psbox_backend_destroy(psbox_backend); psbox_backend = NULL; }
    if (psbox) { x4_psbox_destroy(psbox); psbox = NULL; }
#endif
    printf("Nexo X4: cerrar acceso Microsoft\n");
    int auth_rc = x4_auth_close(auth);
    if (auth_rc >= 0) {
        auth = NULL;
        catalog_cursor = (X4AuthCatalogCursor){0};
        x4_catalog_browser_init(&catalog_browser);
        memset(&catalog, 0, sizeof(catalog));
        memset(&account, 0, sizeof(account));
        memset(&session, 0, sizeof(session));
    }
    printf("Nexo X4: cerrar audio\n");
    x4_audio_stop(&audio);
    printf("Nexo X4: cerrar decoder\n");
    x4_video_stop(&video);
    printf("Nexo X4: cerrar control\n");
    x4_controller_close(&controller);
    printf("Nexo X4: cerrar pantalla\n");
    x4_display_close(&display);
    printf("Nexo X4: recursos cerrados\n");
    int exit_rc = x4_exit_prepare();
    if (exit_rc >= 0) exit_rc = x4_exit_request();
    if (exit_rc >= 0) {
        /* LoadExec is asynchronous; permit recovery if the shell never exits. */
        for (unsigned wait = 0; wait < 100; ++wait) sceKernelUsleep(100000);
        exit_rc = -10;
        printf("Nexo X4: tiempo de salida agotado\n");
    }
    /* A rejected/timed-out request reopens the interface and permits a retry. */
    screen.exit_error = exit_rc;
    screen.exit_requested = 0;
    screen.page = 0;
    media_active = 0;
    live_presented = false;
    overlay_at = input_previous = input_report_at = 0;
    input_intervals = input_interval_us = input_interval_max_us = idle_present_skips = 0;
    closing = 0;
    session_back = 0;
    game_controls = 0;
    if (!auth) auth = x4_auth_create();
    sceKernelUsleep(250000);
    goto reopen_interface;
}
