/* SPDX-License-Identifier: GPL-3.0-only */
#include "../src/ui/psbox/prefs.h"
#include <cassert>
#include <fstream>
#include <cstdlib>
#include <unistd.h>
#include <cstdio>
static void write(const std::string& dir,const std::string& data) {
    std::ofstream file(dir+"/prefs-v1.txt");file<<data;
}
int main() {
    char path[]="/tmp/nexo-prefs-XXXXXX"; assert(mkdtemp(path));std::string dir(path);
    X4PsboxPrefs p;
    write(dir,"X4PSBOX1\nlanguage=2\nsort=1\nhidden=TEST123\n");
    assert(x4_psbox_prefs_load(dir,p));assert(p.stream_profile==0&&!p.diagnostics&&p.hidden.size()==1);
    p.stream_profile=1;p.diagnostics=true;assert(x4_psbox_prefs_save(dir,p));
    X4PsboxPrefs restored;assert(x4_psbox_prefs_load(dir,restored));
    assert(restored.stream_profile==1&&restored.diagnostics&&restored.hidden==p.hidden);
    write(dir,"X4PSBOX1\nlanguage=2\nsort=0\nprofile=2\n");assert(!x4_psbox_prefs_load(dir,p));
    write(dir,"X4PSBOX1\nlanguage=2\nsort=0\ndiagnostics=1\ndiagnostics=0\n");assert(!x4_psbox_prefs_load(dir,p));
    unlink((dir+"/prefs-v1.txt").c_str());rmdir(dir.c_str());
    puts("psbox_prefs_test: legacy migration, roundtrip and invalid settings ok");
}
