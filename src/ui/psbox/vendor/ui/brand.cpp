// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 RafaelNGP
#include "ui/brand.h"

#include <array>

namespace xc::ui {

namespace {

constexpr Color kBlue = rgba(56, 152, 255);
constexpr Color kGreen = rgba(89, 226, 130);
constexpr Color kDeepBlue = rgba(7, 19, 37);

struct Point {
    float x;
    float y;
};

// Two hexagonal chain links share a small central overlap. The open centres
// remain clear at the 44-pixel size used by the app header.
constexpr std::array<Point, 6> kLeftLink{{
    {-0.92f, 0.0f}, {-0.60f, -0.50f}, {-0.04f, -0.50f},
    {0.28f, 0.0f}, {-0.04f, 0.50f}, {-0.60f, 0.50f},
}};
constexpr std::array<Point, 6> kRightLink{{
    {0.92f, 0.0f}, {0.60f, -0.50f}, {0.04f, -0.50f},
    {-0.28f, 0.0f}, {0.04f, 0.50f}, {0.60f, 0.50f},
}};

void drawLink(Canvas& c, float cx, float cy, float r, const std::array<Point, 6>& points, Color color) {
    const float thickness = r * 0.17f;
    for (size_t i = 0; i < points.size(); ++i) {
        const Point& a = points[i];
        const Point& b = points[(i + 1) % points.size()];
        c.line(cx + a.x * r, cy + a.y * r, cx + b.x * r, cy + b.y * r, thickness, color);
    }
}

}  // namespace

void drawBrandMark(Canvas& c, float cx, float cy, float r, Color /*color*/) {
    drawLink(c, cx, cy, r, kLeftLink, kBlue);
    drawLink(c, cx, cy, r, kRightLink, kGreen);
}

void drawAppIcon(Canvas& c, const Fonts& fonts) {
    const int s = c.width();
    c.gradientV({0, 0, s, s}, rgba(13, 34, 59), kDeepBlue);
    drawBrandMark(c, s * 0.5f, s * 0.43f, s * 0.34f);
    const char* word = "NEXO X4";
    int px = s / 10;
    int w = fonts.bold.measure(word, px);
    fonts.bold.draw(c, word, (s - w) / 2, static_cast<int>(s * 0.79f), px, rgba(245, 250, 255));
}

void drawHomeArt(Canvas& c, const Fonts& fonts, bool launch) {
    const float w = static_cast<float>(c.width()), h = static_cast<float>(c.height());
    const float cx = launch ? w * 0.5f : w * 0.70f;
    const float cy = launch ? h * 0.42f : h * 0.47f;
    const float r = h * (launch ? 0.17f : 0.22f);

    c.gradientH({0, 0, c.width(), c.height()}, rgba(5, 13, 26), rgba(9, 35, 39));
    for (int i = 32; i > 0; --i) c.fillCircle(cx, cy, r * (1.0f + i * 0.09f), rgba(38, 139, 229, 3));
    for (int i = 12; i > 0; --i) c.fillCircle(cx, cy, r * (1.0f + i * 0.03f), rgba(69, 213, 139, 4));
    c.gradientV({0, 0, c.width(), static_cast<int>(h * 0.25f)}, rgba(0, 0, 0, 120), rgba(0, 0, 0, 0));
    c.gradientV({0, static_cast<int>(h * 0.7f), c.width(), static_cast<int>(h * 0.3f) + 1}, rgba(0, 0, 0, 0),
                rgba(0, 0, 0, 150));
    if (!launch) c.gradientH({0, 0, static_cast<int>(w * 0.45f), c.height()}, rgba(0, 0, 0, 140), rgba(0, 0, 0, 0));

    drawBrandMark(c, cx, cy, r);
    if (launch) {
        const char* name = "NEXO X4";
        int px = static_cast<int>(h * 0.065f);
        int tw = fonts.bold.measure(name, px);
        fonts.bold.draw(c, name, (c.width() - tw) / 2, static_cast<int>(cy + r * 1.45f), px, rgba(245, 250, 255));
    }
}

}  // namespace xc::ui
