// SPDX-License-Identifier: GPL-3.0-only
// Public C bridge to PSBox's actual AppUi/Canvas, with XCloud4's real titles.
#include "bridge.h"
#include "prefs.h"
#include "overlay_copy.h"
#include "queue_view.h"
#include <climits>
#include "ui/app_ui.h"
#include "ui/strings.h"
#include "ui/stream_menu.h"
#include <algorithm>
#include <atomic>
#include <cstring>
#include <map>
#include <memory>
#include <new>
#include <set>
#include <string>
#include <vector>

struct X4PsboxUi {
    xc::ui::Fonts fonts;
    std::atomic<bool> image_ready{false};
    xc::ui::ImageCache images;
    xc::ui::AppUi app;
    xc::ui::StreamMenu menu;
    xc::ui::Canvas canvas;
    std::vector<X4CatalogTitle> titles;
    std::map<std::string, X4PsboxMetadata> metadata;
    std::vector<X4PsboxCuratedRow> curated;
    bool curated_visible = false;
    bool input_since_catalog = false;
    std::string prefs_dir;
    X4PsboxPrefs prefs;
    enum X4AuthState auth_state = X4_AUTH_IDLE;
    enum X4CatalogState catalog_state = X4_CATALOG_IDLE;
    bool session_terminal = false;
    enum X4SessionState previous_session_state = X4_SESSION_IDLE;
    unsigned previous_session_elapsed = 0;
    int initial_queue_seconds = -1;
    unsigned initial_queue_elapsed = 0;

    X4PsboxUi() : images([this] { image_ready.store(true, std::memory_order_release); },
        48u << 20, "/data/xcloud4-cache/psbox", 128u << 20),
        app(fonts, images), menu(fonts), canvas(1920, 1080) {}

    bool compose() {
        std::vector<xc::ui::GameTile> all, owned;
        all.reserve(titles.size());
        for (const auto& item : titles) {
            xc::ui::GameTile tile;
            tile.titleId = item.id;
            tile.productId = item.product_id;
            tile.name = item.name[0] ? item.name : item.id;
            tile.playable = item.entitled != 0;
            auto found = metadata.find(tile.productId);
            if (!tile.productId.empty() && found != metadata.end()) {
                const auto& m = found->second;
                if (m.name[0]) tile.name = m.name;
                tile.publisher = m.publisher;
                tile.description = m.description;
                tile.tileUrl = m.tile_url;
                tile.heroUrl = m.hero_url;
            }
            if (tile.playable) owned.push_back(tile);
            all.push_back(std::move(tile));
        }
        auto upper = [](std::string s) {
            for (char& ch : s) if (ch >= 'a' && ch <= 'z') ch -= 'a' - 'A';
            return s;
        };
        std::map<std::string, size_t> by_product;
        for (size_t i = 0; i < all.size(); ++i)
            if (!all[i].productId.empty()) by_product.emplace(upper(all[i].productId), i);
        std::vector<xc::ui::GameRow> rows;
        rows.reserve(curated.size() + 1);
        for (const auto& source : curated) {
            xc::ui::GameRow row;
            row.title = source.name;
            row.gamePassBadges = false; // no subscription label in X4 snapshot
            std::set<std::string> seen;
            for (size_t i = 0; i < source.count; ++i) {
                std::string pid = upper(source.product_ids[i]);
                auto found = by_product.find(pid);
                if (found != by_product.end() && seen.insert(pid).second)
                    row.tiles.push_back(all[found->second]);
            }
            if (!row.tiles.empty()) rows.push_back(std::move(row));
        }
        xc::ui::GameRow grid;
        grid.title = "Todos los juegos";
        grid.gamePassBadges = false;
        grid.isGrid = true;
        grid.tiles = all;
        rows.push_back(std::move(grid));
        bool has_curated_rows = rows.size() > 1;
        app.setRows(std::move(rows));
        app.setOwned(owned, {}, true);
        app.setSearchPools(std::move(all), std::move(owned));
        return has_curated_rows;
    }
};

static bool terminated(const char* text, size_t cap) {
    return text && std::memchr(text, 0, cap);
}

extern "C" X4PsboxUi* x4_psbox_create(const char* font_dir) {
    if (!font_dir || !*font_dir) return nullptr;
    std::unique_ptr<X4PsboxUi> ui(new (std::nothrow) X4PsboxUi());
    if (!ui) return nullptr;
    xc::ui::setLanguage(xc::ui::Language::Spanish);
    if (!ui->fonts.load(font_dir)) return nullptr;
    xc::ui::SettingsChoice settings;
    settings.language = 2; settings.resolution = 1;
    ui->app.setSettings(settings);
    ui->app.showSplash("Pulsa X para iniciar sesión");
    return ui.release();
}

extern "C" void x4_psbox_destroy(X4PsboxUi* ui) { delete ui; }

extern "C" int x4_psbox_set_prefs_dir(X4PsboxUi* ui, const char* directory) {
    if (!ui || !directory) return -1;
    X4PsboxPrefs loaded;
    if (!x4_psbox_prefs_load(directory, loaded)) return -1;
    ui->prefs_dir = directory;
    ui->prefs = std::move(loaded);
    xc::ui::setLanguage(static_cast<xc::ui::Language>(ui->prefs.language));
    xc::ui::SettingsChoice settings;
    settings.language = ui->prefs.language;
    settings.resolution = ui->prefs.stream_profile ? 0 : 1;
    settings.diagnostics = ui->prefs.diagnostics;
    ui->menu.setPlaybackOptions(ui->prefs.stream_profile, ui->prefs.diagnostics);
    ui->app.setSettings(settings);
    ui->app.setPrefs(ui->prefs.hidden, static_cast<xc::ui::LibrarySort>(ui->prefs.sort));
    ui->app.invalidate();
    return 0;
}

extern "C" int x4_psbox_set_catalog(X4PsboxUi* ui, const X4CatalogSnapshot* catalog) {
    if (!ui || !catalog || catalog->count > X4_CATALOG_MAX) return -1;
    ui->catalog_state = catalog->state;
    if (catalog->state == X4_CATALOG_LOADING) {
        ui->app.showSplash(catalog->stage[0] ? catalog->stage : "Cargando catálogo");
        return 0;
    }
    if (catalog->state == X4_CATALOG_ERROR) {
        ui->app.showError(catalog->stage[0] ? catalog->stage : "No se pudo cargar el catálogo");
        return 0;
    }
    if (catalog->state != X4_CATALOG_READY || catalog->truncated) return -1;
    for (size_t i = 0; i < catalog->count; ++i) {
        const auto& title = catalog->titles[i];
        if (!terminated(title.id, sizeof(title.id)) ||
            !terminated(title.product_id, sizeof(title.product_id)) ||
            !terminated(title.name, sizeof(title.name))) return -1;
    }
    ui->titles.assign(catalog->titles, catalog->titles + catalog->count);
    ui->metadata.clear(); // public metadata is tied to this catalog revision
    ui->curated.clear();
    ui->curated_visible = false;
    ui->input_since_catalog = false;
    (void)ui->compose();
    if (catalog->region[0]) ui->app.setRegions({catalog->region}, catalog->region);
    ui->app.showHome();
    return 0;
}

extern "C" int x4_psbox_set_metadata(X4PsboxUi* ui, const X4PsboxMetadata* batch, size_t count) {
    if (!ui || (count && !batch) || count > X4_CATALOG_MAX) return -1;
    for (size_t i = 0; i < count; ++i) {
        const auto& m = batch[i];
        if (!terminated(m.product_id, sizeof(m.product_id)) ||
            !terminated(m.name, sizeof(m.name)) || !terminated(m.publisher, sizeof(m.publisher)) ||
            !terminated(m.description, sizeof(m.description)) ||
            !terminated(m.tile_url, sizeof(m.tile_url)) || !terminated(m.hero_url, sizeof(m.hero_url))) return -1;
    }
    for (size_t i = 0; i < count; ++i) {
        const auto& m = batch[i];
        if (!m.product_id[0]) continue;
        bool known = std::any_of(ui->titles.begin(), ui->titles.end(), [&](const X4CatalogTitle& title) {
            return !std::strcmp(title.product_id, m.product_id);
        });
        if (known) ui->metadata[m.product_id] = m;
    }
    if (ui->catalog_state == X4_CATALOG_READY) (void)ui->compose();
    return 0;
}

extern "C" int x4_psbox_set_rows(X4PsboxUi* ui, const X4PsboxCuratedRow* rows, size_t count) {
    if (!ui || (count && !rows) || count > X4_PSB_CURATED_MAX_ROWS) return -1;
    std::vector<X4PsboxCuratedRow> accepted;
    accepted.reserve(count);
    for (size_t r = 0; r < count; ++r) {
        const auto& row = rows[r];
        if (!terminated(row.name, sizeof(row.name)) || !row.name[0] ||
            !row.count || row.count > X4_PSB_CURATED_MAX_ITEMS ||
            row.truncated || row.source_count < row.count) continue;
        bool valid = true;
        for (size_t i = 0; i < row.count; ++i)
            if (!terminated(row.product_ids[i], sizeof(row.product_ids[i])) ||
                !row.product_ids[i][0]) { valid = false; break; }
        if (valid) accepted.push_back(row);
    }
    if (count && accepted.empty()) return 0; // optional failure cannot hide previously valid rows
    if (count) ui->curated = std::move(accepted);
    else ui->curated.clear();
    if (ui->catalog_state == X4_CATALOG_READY) {
        bool had_visible = ui->curated_visible;
        bool visible = ui->compose();
        ui->curated_visible = visible;
        if (!had_visible && visible && !ui->input_since_catalog &&
            ui->app.screen() == xc::ui::Screen::Home &&
            ui->app.tab() == xc::ui::Tab::GamePass)
            ui->app.focusFirstRow();
    }
    return 0;
}

extern "C" void x4_psbox_set_auth(X4PsboxUi* ui, const X4AuthSnapshot* auth) {
    if (!ui || !auth) return;
    ui->auth_state = auth->state;
    if (auth->state == X4_AUTH_WAITING) {
        ui->app.showSignIn(auth->user_code, auth->verification_uri);
    } else if (auth->state == X4_AUTH_ERROR || auth->state == X4_AUTH_DENIED || auth->state == X4_AUTH_EXPIRED) {
        ui->app.showError(auth->stage[0] ? auth->stage : "No se pudo iniciar sesión");
    } else if (auth->state == X4_AUTH_CONNECTING || auth->state == X4_AUTH_CONNECTED) {
        ui->app.showSplash(auth->stage[0] ? auth->stage : "Conectando");
    } else if (auth->state == X4_AUTH_IDLE || auth->state == X4_AUTH_CANCELLED) {
        xc::ui::SettingsChoice settings;
    settings.language = 2; settings.resolution = 1;
    ui->app.setSettings(settings);
    ui->app.showSplash("Pulsa X para iniciar sesión");
    }
}

extern "C" void x4_psbox_set_launch(X4PsboxUi* ui, const char* title_id, const char* status) {
    if (!ui) return;
    // A late close/status path may run after the terminal snapshot has already
    // restored Home. Only a new active session can reopen the launch screen.
    if (ui->session_terminal) return;
    xc::ui::GameTile game;
    if (title_id && ui->app.findTile(title_id, game)) ui->app.showLaunching(game, status ? status : "");
    else ui->app.showSplash(status ? status : "Iniciando juego");
}

extern "C" void x4_psbox_set_session(X4PsboxUi* ui, const X4SessionSnapshot* session,
                                       const char* title_id) {
    if (!ui || !session) return;
    const bool reset_queue = session->state == X4_SESSION_IDLE ||
        session->elapsed_seconds < ui->previous_session_elapsed ||
        (session->state == X4_SESSION_STARTING && ui->previous_session_state != X4_SESSION_STARTING);
    if (reset_queue) {
        ui->initial_queue_seconds = -1;
        ui->initial_queue_elapsed = 0;
        ui->app.setQueueInfo("");
    }
    ui->previous_session_state = session->state;
    ui->previous_session_elapsed = session->elapsed_seconds;
    const auto& estimate = session->queue_estimate;
    auto age = [session](unsigned received) -> int {
        if (received > session->elapsed_seconds) return 0;
        unsigned delta = session->elapsed_seconds - received;
        return delta > INT_MAX ? INT_MAX : static_cast<int>(delta);
    };
    if (ui->initial_queue_seconds < 0 && (estimate.present_mask & X4_QUEUE_TOTAL)) {
        ui->initial_queue_seconds = static_cast<int>(estimate.total_seconds);
        ui->initial_queue_elapsed = estimate.updated_elapsed_seconds;
    }
    std::string queue_details;
    if (session->state == X4_SESSION_WAITING) {
        X4QueueViewInput view{};
        view.elapsed_seconds = session->elapsed_seconds;
        view.allocation_seconds = estimate.present_mask & X4_QUEUE_ALLOCATION ?
            static_cast<int>(estimate.allocation_seconds) : -1;
        view.provisioning_seconds = estimate.present_mask & X4_QUEUE_PROVISIONING ?
            static_cast<int>(estimate.provisioning_seconds) : -1;
        view.total_seconds = estimate.present_mask & X4_QUEUE_TOTAL ?
            static_cast<int>(estimate.total_seconds) : -1;
        switch (estimate.result) {
        case X4_QUEUE_AVAILABLE: view.estimate_status = X4_QUEUE_EST_AVAILABLE; break;
        case X4_QUEUE_UNAVAILABLE: view.estimate_status = X4_QUEUE_EST_UNAVAILABLE; break;
        case X4_QUEUE_ERROR: view.estimate_status = X4_QUEUE_EST_ERROR; break;
        default: view.estimate_status = X4_QUEUE_EST_NONE; break;
        }
        view.estimate_age_seconds = estimate.present_mask ? age(estimate.updated_elapsed_seconds) : -1;
        view.estimate_stale = estimate.result != X4_QUEUE_AVAILABLE || view.estimate_age_seconds > 60 ||
            session->remote_state != X4_REMOTE_WAITING_FOR_RESOURCES;
        view.initial_estimate_seconds = ui->initial_queue_seconds;
        view.initial_estimate_age_seconds = ui->initial_queue_seconds >= 0 ? age(ui->initial_queue_elapsed) : -1;
        view.last_poll_age_seconds = session->state_poll_count ? age(session->last_state_poll_elapsed_seconds) : -1;
        view.seconds_left = session->seconds_left > INT_MAX ? INT_MAX : static_cast<int>(session->seconds_left);
        view.region = session->region;
        X4QueueViewText text{};
        x4_queue_view_format(&view, &text);
        for (unsigned i = 0; i < text.count; ++i) {
            if (i) queue_details += '\n';
            queue_details += text.lines[i];
        }
    }
    const char* stage = session->stage[0] ? session->stage : "Preparando sesión";
    switch (session->state) {
    case X4_SESSION_STARTING: case X4_SESSION_WAITING: case X4_SESSION_READY:
    case X4_SESSION_AUTHORIZING: case X4_SESSION_AUTHORIZED:
    case X4_SESSION_NEGOTIATING: case X4_SESSION_CONNECTING:
        ui->session_terminal = false;
        x4_psbox_set_launch(ui, title_id, stage);
        ui->app.setQueueInfo(queue_details);
        break;
    case X4_SESSION_STREAMING:
        ui->session_terminal = false;
        ui->app.showStreaming();
        break;
    case X4_SESSION_ERROR: {
        ui->session_terminal = true;
        xc::ui::GameTile game;
        if (title_id && ui->app.findTile(title_id, game)) ui->app.showPlayError(stage, game);
        else ui->app.showError(stage);
        break;
    }
    case X4_SESSION_STOPPING:
        ui->session_terminal = false;
        x4_psbox_set_launch(ui, title_id, stage);
        ui->app.setQueueInfo(queue_details);
        break;
    case X4_SESSION_CLOSED: case X4_SESSION_CANCELLED:
        ui->session_terminal = true;
        if (ui->catalog_state == X4_CATALOG_READY) ui->app.showHome();
        break;
    case X4_SESSION_IDLE:
        break;
    }
}

extern "C" X4PsboxEvent x4_psbox_handle(X4PsboxUi* ui, const X4PsboxPad* pad) {
    X4PsboxEvent out{};
    if (!ui || !pad) return out;
    if (ui->catalog_state == X4_CATALOG_READY &&
        (pad->pressed || pad->left_x > 0.5f || pad->left_x < -0.5f ||
         pad->left_y > 0.5f || pad->left_y < -0.5f))
        ui->input_since_catalog = true;
    if ((ui->auth_state == X4_AUTH_IDLE || ui->auth_state == X4_AUTH_CANCELLED ||
         ui->auth_state == X4_AUTH_ERROR || ui->auth_state == X4_AUTH_EXPIRED) &&
        (pad->pressed & X4_PSB_CROSS)) {
        out.action = X4_PSB_ACTION_SIGN_IN;
        return out;
    }
    if (ui->app.screen() == xc::ui::Screen::Details && (pad->pressed & X4_PSB_OPTIONS)) {
        out.action = X4_PSB_ACTION_UNSUPPORTED_SETTING;
        return out;
    }
    xc::ui::NavInput nav;
    nav.up = (pad->pressed & X4_PSB_UP) != 0;
    nav.down = (pad->pressed & X4_PSB_DOWN) != 0;
    nav.left = (pad->pressed & X4_PSB_LEFT) != 0;
    nav.right = (pad->pressed & X4_PSB_RIGHT) != 0;
    nav.accept = (pad->pressed & X4_PSB_CROSS) != 0;
    nav.back = (pad->pressed & X4_PSB_CIRCLE) != 0;
    nav.options = (pad->pressed & X4_PSB_OPTIONS) != 0;
    nav.l1 = (pad->pressed & X4_PSB_L1) != 0;
    nav.r1 = (pad->pressed & X4_PSB_R1) != 0;
    nav.square = (pad->pressed & X4_PSB_SQUARE) != 0;
    nav.triangle = (pad->pressed & X4_PSB_TRIANGLE) != 0;
    nav.r3 = (pad->pressed & X4_PSB_R3) != 0;
    nav.l2 = (pad->pressed & X4_PSB_L2) != 0;
    nav.r2 = (pad->pressed & X4_PSB_R2) != 0;
    nav.touchpad = (pad->held & X4_PSB_TOUCHPAD) != 0;
    nav.dpadLeft = nav.left; nav.dpadRight = nav.right;
    nav.stickX = nav.rawLX = pad->left_x;
    nav.stickY = nav.rawLY = pad->left_y;
    nav.rawRX = pad->right_x; nav.rawRY = pad->right_y;
    nav.l2Analog = pad->l2; nav.r2Analog = pad->r2;
    nav.nowMs = pad->now_ms;
    xc::ui::UiEvent event = ui->app.handle(nav);
    if (ui->app.tab() == xc::ui::Tab::Consoles) {
        ui->app.showTab(xc::ui::Tab::GamePass);
        ui->app.showHome("Consolas remotas no disponibles en PS4");
    }
    switch (event.action) {
    case xc::ui::Action::Play:
        for (unsigned i = 0; i < ui->titles.size(); ++i) {
            if (event.game.titleId == ui->titles[i].id && ui->titles[i].entitled) {
                out.action = X4_PSB_ACTION_PLAY;
                out.catalog_index = i;
                std::memcpy(out.title_id, ui->titles[i].id, sizeof(out.title_id));
                break;
            }
        }
        break;
    case xc::ui::Action::Retry:
        out.action = ui->auth_state == X4_AUTH_AUTHORIZED ? X4_PSB_ACTION_REFRESH_CATALOG : X4_PSB_ACTION_SIGN_IN;
        break;
    case xc::ui::Action::SignOut: out.action = X4_PSB_ACTION_SIGN_OUT; break;
    case xc::ui::Action::CancelLaunch:
        if (ui->session_terminal) {
            if (ui->catalog_state == X4_CATALOG_READY) ui->app.showHome();
        } else {
            out.action = X4_PSB_ACTION_CANCEL_SESSION;
        }
        break;
    case xc::ui::Action::SettingsChanged:
        if (event.settings.language >= 0 &&
            event.settings.language < static_cast<int>(xc::ui::Language::Count)) {
            xc::ui::setLanguage(static_cast<xc::ui::Language>(event.settings.language));
            ui->prefs.language = event.settings.language;
            ui->prefs.stream_profile = event.settings.resolution == 0 ? 1 : 0;
            ui->prefs.diagnostics = event.settings.diagnostics;
            ui->menu.setPlaybackOptions(ui->prefs.stream_profile, ui->prefs.diagnostics);
            if (!ui->prefs_dir.empty() && !x4_psbox_prefs_save(ui->prefs_dir, ui->prefs))
                out.action = X4_PSB_ACTION_PREFS_SAVE_FAILED;
            ui->app.invalidate();
        }
        break;
    case xc::ui::Action::PrefsChanged:
        ui->prefs.hidden = event.hidden;
        ui->prefs.sort = static_cast<int>(event.librarySort);
        if (!ui->prefs_dir.empty() && !x4_psbox_prefs_save(ui->prefs_dir, ui->prefs))
            out.action = X4_PSB_ACTION_PREFS_SAVE_FAILED;
        break;
    case xc::ui::Action::GameSettingsChanged:
    case xc::ui::Action::LoginCountry:
        out.action = X4_PSB_ACTION_UNSUPPORTED_SETTING;
        ui->app.showHome("Ajuste no disponible en PS4");
        break;
    default: break;
    }
    return out;
}

extern "C" int x4_psbox_render(X4PsboxUi* ui, uint32_t* pixels, size_t pixel_count, uint64_t now_ms) {
    if (!ui || !pixels || pixel_count < 1920u * 1080u) return -1;
    if (ui->image_ready.exchange(false, std::memory_order_acq_rel)) ui->app.invalidate();
    if (ui->app.needsRedraw(now_ms)) ui->app.render(ui->canvas, now_ms);
    std::memcpy(pixels, ui->canvas.data(), 1920u * 1080u * sizeof(uint32_t));
    return 0;
}

extern "C" int x4_psbox_screen(const X4PsboxUi* ui) {
    return ui ? static_cast<int>(ui->app.screen()) : -1;
}

extern "C" int x4_psbox_tab(const X4PsboxUi* ui) {
    return ui ? static_cast<int>(ui->app.tab()) : -1;
}

extern "C" int x4_psbox_detail_wanted(X4PsboxUi* ui, char product_id[32], uint64_t now_ms) {
    if (!ui || !product_id) return -1;
    product_id[0] = 0;
    const std::string wanted = ui->app.detailWanted(now_ms);
    if (wanted.empty() || wanted.size() >= 32) return 0;
    for (const auto& title : ui->titles) {
        if (wanted == title.product_id) {
            std::memcpy(product_id, wanted.c_str(), wanted.size() + 1);
            return 1;
        }
    }
    return 0;
}

extern "C" int x4_psbox_stream_profile(const X4PsboxUi *ui) {
    return ui ? ui->prefs.stream_profile : 0;
}
extern "C" int x4_psbox_diagnostics(const X4PsboxUi *ui) {
    return ui && ui->prefs.diagnostics;
}
extern "C" int x4_psbox_menu_open(const X4PsboxUi* ui) {
    return ui && ui->menu.isOpen();
}

extern "C" int x4_psbox_needs_overlay(const X4PsboxUi* ui, int degraded) {
    return ui && (ui->menu.isOpen() || ui->menu.statsOn() || degraded);
}

extern "C" void x4_psbox_menu_toggle(X4PsboxUi* ui) {
    if (!ui) return;
    if (ui->menu.isOpen()) ui->menu.close();
    else ui->menu.open(0, ui->menu.statsOn(), 0, 1, 0, false, false);
}

extern "C" enum X4PsboxMenuAction x4_psbox_menu_handle(X4PsboxUi* ui, const X4PsboxPad* pad) {
    if (!ui || !pad || !ui->menu.isOpen()) return X4_PSB_MENU_NONE;
    xc::ui::NavInput nav;
    nav.up = (pad->pressed & X4_PSB_UP) != 0;
    nav.down = (pad->pressed & X4_PSB_DOWN) != 0;
    nav.left = (pad->pressed & X4_PSB_LEFT) != 0;
    nav.right = (pad->pressed & X4_PSB_RIGHT) != 0;
    nav.accept = (pad->pressed & X4_PSB_CROSS) != 0;
    nav.back = (pad->pressed & X4_PSB_CIRCLE) != 0;
    nav.options = (pad->pressed & X4_PSB_OPTIONS) != 0;
    switch (ui->menu.handle(nav)) {
    case xc::ui::MenuAction::Close: return X4_PSB_MENU_CLOSE;
    case xc::ui::MenuAction::Leave: return X4_PSB_MENU_LEAVE;
    case xc::ui::MenuAction::XboxButton: return X4_PSB_MENU_GUIDE;
    case xc::ui::MenuAction::Stats: return X4_PSB_MENU_STATS;
    case xc::ui::MenuAction::ProfileChanged:
    case xc::ui::MenuAction::DiagnosticsChanged: {
        int old_profile = ui->prefs.stream_profile;
        ui->prefs.stream_profile = ui->menu.playbackProfile();
        ui->prefs.diagnostics = ui->menu.diagnosticsOn();
        xc::ui::SettingsChoice settings;
        settings.language = ui->prefs.language;
        settings.resolution = ui->prefs.stream_profile ? 0 : 1;
        settings.diagnostics = ui->prefs.diagnostics;
        ui->app.setSettings(settings);
        if (!ui->prefs_dir.empty() && !x4_psbox_prefs_save(ui->prefs_dir, ui->prefs))
            return X4_PSB_MENU_PREFS_FAILED;
        return old_profile != ui->prefs.stream_profile ? X4_PSB_MENU_PROFILE : X4_PSB_MENU_DIAGNOSTICS;
    }
    default: return X4_PSB_MENU_NONE;
    }
}

/* Canvas layers are opaque on PS4. Copy only their rectangles directly
 * into VideoOut; alpha blending would read slow WC memory on every frame. */
static void composite_canvas(uint32_t* target, const xc::ui::Canvas& source,
                             int left, int top) {
    x4_overlay_copy(target, 1920, 1080, source.data(), source.width(), source.height(), left, top);
}

extern "C" int x4_psbox_menu_composite(X4PsboxUi* ui, uint32_t* pixels,
    size_t pixel_count, const X4PsboxStreamInfo* info) {
    if (!ui || !pixels || !info || pixel_count < 1920u * 1080u ||
        !terminated(info->region, sizeof(info->region))) return -1;
    xc::ui::StreamInfo visible;
    visible.region = info->region;
    visible.width = info->width;
    visible.height = info->height;
    visible.rttMs = info->rtt_ms;
    visible.fps = info->fps;
    visible.mbps = info->mbps;
    visible.lossPct = info->loss_pct;
    visible.decodeMs = info->decode_ms;
    visible.onScreenMs = info->on_screen_ms;
    visible.degraded = info->degraded != 0;
    if (ui->menu.isOpen()) {
        const xc::ui::Canvas& layer = ui->menu.renderMenu(visible);
        composite_canvas(pixels, layer, xc::ui::StreamMenu::kMenuX,
            xc::ui::StreamMenu::kMenuY);
    } else if (ui->menu.statsOn()) {
        const xc::ui::Canvas& layer = ui->menu.renderStats(visible);
        composite_canvas(pixels, layer, xc::ui::StreamMenu::kStatsX,
            xc::ui::StreamMenu::kStatsY);
    } else if (visible.degraded) {
        const xc::ui::Canvas& layer = ui->menu.renderWarning();
        composite_canvas(pixels, layer, xc::ui::StreamMenu::kWarnRight - layer.width(),
            xc::ui::StreamMenu::kWarnY);
    }
    return 0;
}
