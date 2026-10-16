/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <string>
#include <vector>

struct X4PsboxPrefs {
    int language = 2; // Spanish
    int sort = 0;
    std::vector<std::string> hidden;
};
bool x4_psbox_prefs_load(const std::string& dir, X4PsboxPrefs& out);
bool x4_psbox_prefs_save(const std::string& dir, const X4PsboxPrefs& prefs);
