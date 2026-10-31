// SPDX-License-Identifier: GPL-3.0-only
// Exercise the real bridge composition and navigation without HTTP, disk cache,
// accounts or credentials. Including the implementation exposes its public-only
// cache model for assertions; compile this file instead of bridge.cpp.
#include "../src/ui/psbox/bridge.cpp"
#include "util/log.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
#include <unistd.h>

namespace xc::platform {
bool readFile(const std::string& path, std::string& out) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;
    out.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    return true;
}
}
namespace xc::log { void write(Level, const char*, ...) {} }
namespace xc::ui {
ImageCache::ImageCache(std::function<void()>, size_t budget, std::string, size_t diskBudget)
    : budget_(budget), diskBudget_(diskBudget) {}
ImageCache::~ImageCache() = default;
std::shared_ptr<const Image> ImageCache::get(const std::string&, int, int) { return nullptr; }
}

static void copy(char* destination, size_t cap, const std::string& source) {
    assert(source.size() < cap);
    std::memcpy(destination, source.c_str(), source.size() + 1);
}

static X4CatalogTitle title(const char* id, const char* product, int entitled) {
    X4CatalogTitle result{};
    copy(result.id, sizeof(result.id), id);
    copy(result.product_id, sizeof(result.product_id), product);
    result.entitled = entitled;
    return result;
}

static X4PsboxMetadata public_metadata(const char* product, int language) {
    X4PsboxMetadata metadata{};
    copy(metadata.product_id, sizeof(metadata.product_id), product);
    copy(metadata.name, sizeof(metadata.name), "Public title " + std::to_string(language) + " " + product);
    copy(metadata.tile_url, sizeof(metadata.tile_url), std::string("https://public.invalid/") + product + ".png");
    copy(metadata.hero_url, sizeof(metadata.hero_url), std::string("https://public.invalid/") + product + "-hero.png");
    return metadata;
}

static X4PsboxEvent press(X4PsboxUi* ui, uint32_t button) {
    X4PsboxPad pad{};
    pad.pressed = button;
    pad.now_ms = 5000;
    return x4_psbox_handle(ui, &pad);
}

int main(int argc, char** argv) {
    assert(argc == 2);
    auto catalog = std::make_unique<X4CatalogSnapshot>();
    for (int language = 0; language < static_cast<int>(xc::ui::Language::Count); ++language) {
        char directory[] = "/tmp/nexo-public-refresh-XXXXXX";
        assert(mkdtemp(directory));
        X4PsboxPrefs prefs;
        prefs.language = language;
        assert(x4_psbox_prefs_save(directory, prefs));
        std::unique_ptr<X4PsboxUi, decltype(&x4_psbox_destroy)> ui(x4_psbox_create(argv[1]), x4_psbox_destroy);
        assert(ui && !x4_psbox_set_prefs_dir(ui.get(), directory));
        X4AuthSnapshot auth{};
        auth.state = X4_AUTH_AUTHORIZED;
        x4_psbox_set_auth(ui.get(), &auth);
        *catalog = {};
        catalog->state = X4_CATALOG_READY;
        catalog->count = catalog->total = 3;
        catalog->titles[0] = title("old-A", "PRODUCT-A", 1);
        catalog->titles[1] = title("old-B", "PRODUCT-B", 1);
        catalog->titles[2] = title("old-C", "PRODUCT-C", 0);
        catalog->regions.count = 2;
        copy(catalog->regions.items[0].id, sizeof(catalog->regions.items[0].id), "EastUS");
        catalog->regions.items[0].is_default = 1;
        copy(catalog->regions.items[1].id, sizeof(catalog->regions.items[1].id), "BrazilSouth");
        assert(!x4_psbox_set_catalog(ui.get(), catalog.get()));
        X4PsboxMetadata metadata[] = {public_metadata("PRODUCT-A", language),
            public_metadata("PRODUCT-B", language), public_metadata("PRODUCT-C", language)};
        assert(!x4_psbox_set_metadata(ui.get(), metadata, 3));
        X4PsboxCuratedRow row{};
        copy(row.name, sizeof(row.name), "Public curated " + std::to_string(language));
        row.count = row.source_count = 4;
        copy(row.product_ids[0], sizeof(row.product_ids[0]), "PRODUCT-B");
        copy(row.product_ids[1], sizeof(row.product_ids[1]), "PRODUCT-A");
        copy(row.product_ids[2], sizeof(row.product_ids[2]), "UNVERIFIED");
        copy(row.product_ids[3], sizeof(row.product_ids[3]), "PRODUCT-B");
        assert(!x4_psbox_set_rows(ui.get(), &row, 1));
        assert(ui->curated_visible);

        // Reproduce the native trigger: save region and diagnostics in Settings.
        assert(press(ui.get(), X4_PSB_OPTIONS).action == X4_PSB_ACTION_NONE);
        press(ui.get(), X4_PSB_DOWN);
        press(ui.get(), X4_PSB_DOWN);
        press(ui.get(), X4_PSB_CROSS);
        press(ui.get(), X4_PSB_DOWN);
        press(ui.get(), X4_PSB_DOWN);
        press(ui.get(), X4_PSB_CROSS);
        press(ui.get(), X4_PSB_DOWN);
        press(ui.get(), X4_PSB_RIGHT);
        assert(press(ui.get(), X4_PSB_CIRCLE).action == X4_PSB_ACTION_REGION_CHANGED);
        assert(x4_psbox_language(ui.get()) == language);
        assert(!std::strcmp(x4_psbox_region(ui.get()), "BrazilSouth"));
        assert(x4_psbox_diagnostics(ui.get()));
        // Parent/backend replay LOADING -> same verified products, new region.
        // No metadata or curated replay follows: the backend signature dedupes.
        catalog->state = X4_CATALOG_LOADING;
        assert(!x4_psbox_set_catalog(ui.get(), catalog.get()));
        catalog->state = X4_CATALOG_READY;
        copy(catalog->region, sizeof(catalog->region), "BrazilSouth");
        assert(!x4_psbox_set_catalog(ui.get(), catalog.get()));
        assert(ui->curated_visible && ui->metadata.size() == 3);
        xc::ui::GameTile tile;
        assert(ui->app.firstTile(tile));
        assert(tile.productId == "PRODUCT-B" && tile.titleId == "old-B");
        assert(tile.name == metadata[1].name && tile.tileUrl == metadata[1].tile_url);
        assert(tile.heroUrl == metadata[1].hero_url && tile.playable);
        ui->app.render(ui->canvas, 5000);

        // Changed service snapshot: keep only matching public metadata, replace
        // service IDs/entitlements, and intersect curated lists with current IDs.
        catalog->count = catalog->total = 2;
        catalog->titles[0] = title("new-B", "PRODUCT-B", 0);
        catalog->titles[1] = title("new-D", "PRODUCT-D", 1);
        assert(!x4_psbox_set_catalog(ui.get(), catalog.get()));
        assert(ui->metadata.size() == 1 && ui->metadata.count("PRODUCT-B"));
        assert(ui->curated_visible);
        assert(ui->app.firstTile(tile) && tile.productId == "PRODUCT-B");
        assert(tile.titleId == "new-B" && !tile.playable && tile.name == metadata[1].name);
        assert(!ui->app.findTile("old-A", tile) && !ui->app.findTile("old-B", tile));
        assert(!ui->app.findTile("old-C", tile));
        assert(ui->app.findTile("new-D", tile) && tile.playable && tile.name == "new-D");
        assert(tile.tileUrl.empty() && tile.heroUrl.empty());
        // Late results for removed products cannot rehydrate an absent product.
        assert(!x4_psbox_set_metadata(ui.get(), metadata, 3));
        assert(ui->metadata.size() == 1);
        ui->app.focusFirstRow();
        press(ui.get(), X4_PSB_CROSS);
        assert(press(ui.get(), X4_PSB_CROSS).action != X4_PSB_ACTION_PLAY);
        press(ui.get(), X4_PSB_CIRCLE);
        press(ui.get(), X4_PSB_DOWN);
        press(ui.get(), X4_PSB_RIGHT);
        press(ui.get(), X4_PSB_CROSS);
        auto play = press(ui.get(), X4_PSB_CROSS);
        assert(play.action == X4_PSB_ACTION_PLAY && play.catalog_index == 1);
        assert(!std::strcmp(play.title_id, "new-D"));

        // Invalid snapshots cannot partly replace the previously verified model.
        catalog->regions.count = X4_REGION_MAX + 1;
        assert(x4_psbox_set_catalog(ui.get(), catalog.get()) == -1);
        assert(ui->metadata.size() == 1 && ui->titles.size() == 2);
        catalog->regions.count = 2;
        catalog->count = catalog->total = 0;
        assert(!x4_psbox_set_catalog(ui.get(), catalog.get()));
        assert(ui->metadata.empty() && !ui->curated_visible);
        assert(!ui->app.firstTile(tile) && !ui->app.findTile("new-B", tile));
        assert(!ui->app.findTile("new-D", tile));
        assert(x4_psbox_language(ui.get()) == language);
        assert(!std::strcmp(x4_psbox_region(ui.get()), "BrazilSouth"));
        assert(x4_psbox_diagnostics(ui.get()));
        X4PsboxPrefs persisted;
        assert(x4_psbox_prefs_load(directory, persisted));
        assert(persisted.language == language && persisted.region == "BrazilSouth" && persisted.diagnostics);
        ui.reset();
        std::string filename = std::string(directory) + "/prefs-v1.txt";
        assert(!unlink(filename.c_str()) && !rmdir(directory));
    }
    std::cout << "psbox_catalog_refresh_test: public caches survive region refresh; current mappings/access and six-language prefs retained\n";
}
