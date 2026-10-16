// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 RafaelNGP
// NEXO X4's original linked blue/green mark is drawn with Canvas so it stays
// sharp in small headers and in generated artwork. See assets/branding/.
#pragma once

#include "ui/canvas.h"
#include "ui/font.h"

namespace xc::ui {

constexpr Color kBrandGreen = rgba(89, 226, 130);

// Draw the two-link mark centred at (cx, cy), scaled by r. The color argument
// remains for callers of the original Canvas API; the mark uses brand colours.
void drawBrandMark(Canvas& c, float cx, float cy, float r, Color color = rgba(255, 255, 255));

// Square app icon and wide home-screen artwork.
void drawAppIcon(Canvas& c, const Fonts& fonts);
void drawHomeArt(Canvas& c, const Fonts& fonts, bool launch);

}  // namespace xc::ui
