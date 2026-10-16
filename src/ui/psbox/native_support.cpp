// SPDX-License-Identifier: GPL-3.0-only
// PS4 file/log adapters for the vendored PSBox renderer. The real artwork
// cache is implemented separately in ui/psbox_backend/image_cache.cpp.
#include "platform/platform.h"
#include "util/log.h"
#include <cstdio>
#include <cstdarg>

namespace xc::platform {
bool readFile(const std::string& path, std::string& out) {
    FILE* file = std::fopen(path.c_str(), "rb");
    if (!file) return false;
    if (std::fseek(file, 0, SEEK_END) != 0) { std::fclose(file); return false; }
    long length = std::ftell(file);
    if (length < 0 || length > 8 * 1024 * 1024 || std::fseek(file, 0, SEEK_SET) != 0) {
        std::fclose(file); return false;
    }
    out.resize(static_cast<size_t>(length));
    bool okay = out.empty() || std::fread(&out[0], 1, out.size(), file) == out.size();
    std::fclose(file);
    if (!okay) out.clear();
    return okay;
}
}

namespace xc::log {
void write(Level, const char* format, ...) {
    va_list args;
    va_start(args, format);
    std::vfprintf(stderr, format, args);
    va_end(args);
    std::fputc('\n', stderr);
}
}
