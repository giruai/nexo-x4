// SPDX-License-Identifier: GPL-3.0-only
// Host-only behavior checks for native PS4 settings; no network or saved-session I/O.
#include "ui/app_ui.h"
#include "ui/stream_menu.h"
#include "ui/strings.h"
#include "util/log.h"

#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>

namespace xc::platform {
bool readFile(const std::string& path, std::string& out) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;
    out.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    return true;
}
}
namespace xc::log {
void write(Level, const char*, ...) {}
}
namespace xc::ui {
// The UI needs only the image cache interface. These stubs never download images.
ImageCache::ImageCache(std::function<void()>, size_t budget, std::string, size_t diskBudget)
    : budget_(budget), diskBudget_(diskBudget) {}
ImageCache::~ImageCache() = default;
std::shared_ptr<const Image> ImageCache::get(const std::string&, int, int) { return nullptr; }
}

using namespace xc::ui;

static NavInput press(bool NavInput::*key) {
    NavInput input{};
    input.*key = true;
    return input;
}

static void openRegion(AppUi& ui) {
    assert(ui.handle(press(&NavInput::options)).action == Action::None);
    assert(ui.screen() == Screen::Settings);
    assert(ui.handle(press(&NavInput::down)).action == Action::None);
    assert(ui.handle(press(&NavInput::down)).action == Action::None);
    assert(ui.handle(press(&NavInput::accept)).action == Action::None);
}

int main(int argc, char** argv) {
    assert(argc == 2);
    Fonts fonts;
    assert(fonts.load(argv[1]));
    ImageCache images([] {});
    AppUi ui(fonts, images);
    Canvas canvas(1920, 1080);
    ui.setRegions({"BRAZILSOUTH", "EASTUS"}, "BRAZILSOUTH");

    for (int lang = 0; lang < static_cast<int>(Language::Count); ++lang) {
        SettingsChoice choice;
        choice.language = lang;
        ui.setSettings(choice);
        setLanguage(static_cast<Language>(lang));
        assert(*tr(Str::ImportSavedSession));
        assert(*tr(Str::ImportSavedSessionNote));
        assert(fonts.semibold.measure(tr(Str::ImportSavedSession), 28) < 1080);
        assert(fonts.regular.wrap(tr(Str::ImportSavedSessionNote), 24, 1200, 20).size() <= 3);
        assert(fonts.regular.wrap(tr(Str::Ps4StreamControlsHint), 22, 1728, 20).size() <= 3);
        assert(fonts.regular.wrap(tr(Str::Ps4MenuControlsHint), 16, StreamMenu::kMenuW - 80, 20).size() <= 2);

        for (Screen origin : {Screen::Splash, Screen::SignIn, Screen::Error}) {
            if (origin == Screen::Splash) ui.showSplash("Sign in");
            else if (origin == Screen::SignIn) ui.showSignIn("ABC123", "https://example.test");
            else ui.showError("Example");
            assert(ui.handle({}).action == Action::None);
            assert(ui.handle(press(&NavInput::options)).action == Action::None);
            assert(ui.screen() == Screen::Settings);
            for (int row = 0; row < 4; ++row)
                assert(ui.handle(press(&NavInput::down)).action == Action::None);
            // Merely reaching, idling on, or pressing sideways on Import cannot import.
            assert(ui.handle({}).action == Action::None);
            assert(ui.handle(press(&NavInput::right)).action == Action::None);
            ui.render(canvas, 1000);
            auto imported = ui.handle(press(&NavInput::accept));
            assert(imported.action == Action::ImportSavedSession);
            assert(imported.settings.language == lang);
            assert(ui.handle({}).action == Action::None);
            assert(ui.handle(press(&NavInput::back)).action == Action::SettingsChanged);
            assert(ui.screen() == origin);
        }

        ui.showSplash("Sign in");
        openRegion(ui);
        ui.handle(press(&NavInput::down));
        ui.handle(press(&NavInput::down));
        assert(ui.handle(press(&NavInput::accept)).action == Action::None);
        auto chosen = ui.handle(press(&NavInput::back));
        assert(chosen.action == Action::SettingsChanged);
        assert(chosen.settings.region == "EASTUS");

        // An unavailable preferred ID stays selected and survives accepting its row.
        choice.region = "UNAVAILABLE";
        ui.setSettings(choice);
        ui.showSplash("Sign in");
        openRegion(ui);
        assert(ui.handle(press(&NavInput::accept)).action == Action::None);
        chosen = ui.handle(press(&NavInput::back));
        assert(chosen.settings.region == "UNAVAILABLE");
        // Moving right from the saved option wraps to Auto and explicitly clears it.
        openRegion(ui);
        ui.handle(press(&NavInput::back)); // close the dropdown without changing the preference
        ui.handle(press(&NavInput::right)); // wraps unavailable preference to Automatic
        chosen = ui.handle(press(&NavInput::back));
        assert(chosen.action == Action::SettingsChanged);
        assert(chosen.settings.region.empty());
    }
    std::cout << "ps4_settings_test: six languages, explicit import, region selection and text fit passed\n";
}
