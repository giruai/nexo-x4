// SPDX-License-Identifier: GPL-3.0-or-later
// Host-only regression test for the PS4 stream menu. Run with the bundled font directory.
#include "ui/stream_menu.h"
#include "ui/strings.h"
#include "platform/platform.h"
#include "util/log.h"

#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>

namespace xc::platform {
bool readFile(const std::string& path, std::string& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    return true;
}
}
namespace xc::log {
void write(Level, const char*, ...) {}
}

using namespace xc::ui;

static NavInput input(bool NavInput::*key) {
    NavInput in{};
    in.*key = true;
    return in;
}

static void markBackground(const Canvas& c, int x, int y) {
    const_cast<uint32_t*>(c.data())[static_cast<size_t>(y) * c.width() + x] = rgba(255, 0, 255);
}

static bool isMarked(const Canvas& c, int x, int y) {
    return c.data()[static_cast<size_t>(y) * c.width() + x] == rgba(255, 0, 255);
}

static void assertOpaque(const Canvas& c) {
    for (size_t i = 0, count = static_cast<size_t>(c.width()) * c.height(); i < count; ++i)
        assert((c.data()[i] >> 24) == 255);
}

int main(int argc, char** argv) {
    assert(argc == 2);
    Fonts fonts;
    assert(fonts.load(argv[1]));
    assert(fonts.semibold.measure("Calidad de video", 22) +
           fonts.semibold.measure("1080p (experimental)", 20) + 48 < StreamMenu::kMenuW - 48);
    assert(fonts.regular.measure("Calidad de video: próxima sesión", 17) < StreamMenu::kMenuW - 80);
    StreamMenu menu(fonts);
    assert(menu.playbackProfile() == 0);
    assert(!menu.diagnosticsOn());
    menu.setPlaybackOptions(7, true);
    assert(menu.playbackProfile() == 1 && menu.diagnosticsOn());
    menu.setPlaybackOptions(0, false);

    menu.open(1, false);
    assert(menu.handle(input(&NavInput::down)) == MenuAction::None);
    assert(menu.handle(input(&NavInput::accept)) == MenuAction::ProfileChanged);
    assert(menu.playbackProfile() == 1 && menu.isOpen());
    assert(menu.handle(input(&NavInput::left)) == MenuAction::ProfileChanged);
    assert(menu.playbackProfile() == 0 && menu.isOpen());
    assert(menu.handle(input(&NavInput::down)) == MenuAction::None);
    assert(menu.handle(input(&NavInput::right)) == MenuAction::DiagnosticsChanged);
    assert(menu.diagnosticsOn() && menu.isOpen());
    assert(menu.handle(input(&NavInput::down)) == MenuAction::None);
    assert(menu.handle(input(&NavInput::accept)) == MenuAction::Stats);
    assert(menu.statsOn() && menu.isOpen());
    assert(menu.handle(input(&NavInput::down)) == MenuAction::None);
    assert(menu.handle(input(&NavInput::left)) == MenuAction::None);
    assert(menu.isOpen());
    assert(menu.handle(input(&NavInput::back)) == MenuAction::Close);
    assert(!menu.isOpen());

    StreamInfo info{};
    info.region = "Brazil South";
    info.rttMs = 31;
    info.fps = 59.91;
    info.mbps = 6.04;
    info.lossPct = 0.02;
    info.decodeMs = 4.04;
    const Canvas& first = menu.renderMenu(info);
    assert(first.width() == StreamMenu::kMenuW && first.height() == StreamMenu::kMenuH);
    assertOpaque(first);
    markBackground(first, 10, 300);
    info.fps = 59.92;  // Same displayed 60 fps: no raster pass.
    assert(&menu.renderMenu(info) == &first && isMarked(first, 10, 300));
    info.fps = 58.0;
    assert(!isMarked(menu.renderMenu(info), 10, 300));
    markBackground(first, 10, 300);
    menu.open(1, false);  // Cursor moves from Leave to Xbox.
    assert(!isMarked(menu.renderMenu(info), 10, 300));
    markBackground(first, 10, 300);
    menu.setPlaybackOptions(1, true);
    assert(!isMarked(menu.renderMenu(info), 10, 300));

    const Canvas& stats = menu.renderStats(info);
    assertOpaque(stats);
    markBackground(stats, 0, 0);
    info.fps = 58.1;  // Same displayed text.
    assert(&menu.renderStats(info) == &stats && isMarked(stats, 0, 0));
    info.mbps = 6.24;
    assert(!isMarked(menu.renderStats(info), 0, 0));

    const Canvas& warning = menu.renderWarning();
    assertOpaque(warning);
    markBackground(warning, 0, 0);
    assert(isMarked(menu.renderWarning(), 0, 0));
    setLanguage(Language::Spanish);
    assert(!isMarked(menu.renderWarning(), 0, 0));

    std::cout << "stream_menu_test: ok\n";
}
