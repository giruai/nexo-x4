// SPDX-License-Identifier: GPL-3.0-only
// Small, account-free PSBox UI preferences. No stat() dependency (native PS4
// stat layout differs from the OpenOrbis header on this target).
#include "prefs.h"
#include <cerrno>
#include <cstdio>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {
constexpr size_t kMaxBytes = 160u * 1024u;
constexpr size_t kMaxHidden = 4096u;

bool directory(const std::string& dir) {
    if (dir.empty() || dir[0] != '/' || dir.size() > 200 || dir.find("..") != std::string::npos) return false;
    return true;
}
bool product(const std::string& id) {
    if (id.empty() || id.size() >= 32) return false;
    for (unsigned char c : id)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))) return false;
    return true;
}
bool writeAll(int fd, const char* data, size_t length) {
    while (length) {
        ssize_t n = ::write(fd, data, length);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return false;
        data += n; length -= static_cast<size_t>(n);
    }
    return true;
}
bool digit(const std::string& line, const char* prefix, int limit, int& out) {
    size_t size = std::char_traits<char>::length(prefix);
    if (line.size() != size + 1 || line.compare(0, size, prefix)) return false;
    char c = line[size];
    if (c < '0' || c >= '0' + limit) return false;
    out = c - '0'; return true;
}
}

bool x4_psbox_prefs_load(const std::string& dir, X4PsboxPrefs& out) {
    out = {};
    if (!directory(dir)) return false;
    std::string file = dir + "/prefs-v1.txt";
    int fd = ::open(file.c_str(), O_RDONLY);
    if (fd < 0) return errno == ENOENT;
    std::string bytes;
    char chunk[2048];
    for (;;) {
        ssize_t n = ::read(fd, chunk, sizeof(chunk));
        if (n < 0 && errno == EINTR) continue;
        if (n < 0 || bytes.size() + static_cast<size_t>(n) > kMaxBytes) { ::close(fd); return false; }
        if (n == 0) break;
        bytes.append(chunk, static_cast<size_t>(n));
    }
    ::close(fd);
    if (bytes.compare(0, 9, "X4PSBOX1\n")) return false;
    size_t pos = 9;
    bool language = false, sort = false, profile = false, diagnostic = false;
    while (pos < bytes.size()) {
        size_t end = bytes.find('\n', pos);
        if (end == std::string::npos) return false;
        std::string line = bytes.substr(pos, end - pos);
        pos = end + 1;
        if (line.compare(0, 9, "language=") == 0) {
            if (language || !digit(line, "language=", 6, out.language)) return false;
            language = true;
        } else if (line.compare(0, 5, "sort=") == 0) {
            if (sort || !digit(line, "sort=", 3, out.sort)) return false;
            sort = true;
        } else if (line.compare(0, 8, "profile=") == 0) {
            if (profile || !digit(line, "profile=", 2, out.stream_profile)) return false;
            profile = true;
        } else if (line.compare(0, 12, "diagnostics=") == 0) {
            int value = 0;
            if (diagnostic || !digit(line, "diagnostics=", 2, value)) return false;
            out.diagnostics = value != 0; diagnostic = true;
        } else if (line.compare(0, 7, "hidden=") == 0) {
            std::string id = line.substr(7);
            if (!product(id) || out.hidden.size() == kMaxHidden) return false;
            out.hidden.push_back(std::move(id));
        } else return false;
    }
    return language && sort;
}

bool x4_psbox_prefs_save(const std::string& dir, const X4PsboxPrefs& prefs) {
    if (!directory(dir) || prefs.language < 0 || prefs.language >= 6 ||
        prefs.sort < 0 || prefs.sort >= 3 || prefs.stream_profile < 0 || prefs.stream_profile > 1 || prefs.hidden.size() > kMaxHidden) return false;
    std::string bytes = "X4PSBOX1\nlanguage=" + std::to_string(prefs.language) +
        "\nsort=" + std::to_string(prefs.sort) + "\nprofile=" + std::to_string(prefs.stream_profile) +
        "\ndiagnostics=" + (prefs.diagnostics ? "1\n" : "0\n");
    for (const auto& id : prefs.hidden) {
        if (!product(id) || bytes.size() + id.size() + 8 > kMaxBytes) return false;
        bytes += "hidden=" + id + "\n";
    }
    if (::mkdir(dir.c_str(), 0700) != 0 && errno != EEXIST) return false;
    std::string temp = dir + "/prefs-v1.tmp", file = dir + "/prefs-v1.txt";
    int fd = ::open(temp.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0) return false;
    bool okay = writeAll(fd, bytes.data(), bytes.size()) && ::fsync(fd) == 0;
    if (::close(fd) != 0) okay = false;
    if (okay) okay = ::rename(temp.c_str(), file.c_str()) == 0;
    if (!okay) ::unlink(temp.c_str());
    return okay;
}
