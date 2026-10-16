// SPDX-License-Identifier: GPL-3.0-only
// Adapted from PSBox image_cache.cpp, Copyright (C) 2026 RafaelNGP.
// PS4 adapter for PSBox's ImageCache interface. Decoder/resize headers are
// Sean Barrett's stb_image and stb_image_resize2 (public domain or MIT).
#include "ui/image_cache.h"
#include "../../auth/http_client.h"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#endif
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STBI_MAX_DIMENSIONS 4096
#include "extern/stb_image.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#define STB_IMAGE_RESIZE_STATIC
#include "extern/stb_image_resize2.h"
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#include <orbis/libkernel.h>
#include <pthread.h>
#include <sys/stat.h>
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <memory>
#include <new>

namespace xc::platform {
namespace {
struct Task { pthread_t thread; std::function<void()> run; };
void* trampoline(void* opaque) {
    static_cast<Task*>(opaque)->run();
    return nullptr;
}
}
uint64_t nowMs() { return sceKernelGetProcessTime() / 1000u; }
bool startThread(Thread& out, std::function<void()> fn, size_t stackBytes) {
    if (out.handle) return false;
    auto* task = new (std::nothrow) Task{};
    if (!task) return false;
    task->run = std::move(fn);
    pthread_attr_t attr;
    if (pthread_attr_init(&attr) != 0) { delete task; return false; }
    int rc = pthread_attr_setstacksize(&attr, stackBytes);
    if (rc == 0) rc = pthread_create(&task->thread, &attr, trampoline, task);
    pthread_attr_destroy(&attr);
    if (rc != 0) { delete task; return false; }
    out.handle = task;
    return true;
}
void Thread::join() {
    auto* task = static_cast<Task*>(handle);
    if (!task) return;
    pthread_join(task->thread, nullptr);
    delete task;
    handle = nullptr;
}
}

namespace xc::ui {
namespace {
constexpr size_t kResponseCap = 2u * 1024u * 1024u;
constexpr size_t kDecodedPixels = 8u * 1024u * 1024u;
constexpr unsigned kDiskSlots = 64;
constexpr uint32_t kDiskMagic = 0x58493450u;
constexpr const char* kImagePrefix = "https://store-images.s-microsoft.com/image/";
struct CacheHeader { uint64_t key; uint32_t size; uint32_t magic; };

uint64_t hashUrl(const std::string& url) {
    uint64_t value = 1469598103934665603ull;
    for (unsigned char ch : url) value = (value ^ ch) * 1099511628211ull;
    return value;
}
bool safeImageUrl(const std::string& url) {
    if (url.size() > 384 || url.compare(0, std::strlen(kImagePrefix), kImagePrefix)) return false;
    if (url.size() == std::strlen(kImagePrefix)) return false;
    for (size_t i = std::strlen(kImagePrefix); i < url.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(url[i]);
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '_' || c == '/')) return false;
    }
    return true;
}
std::string sizedUrl(const std::string& url, int w, int h) {
    if (!safeImageUrl(url) || w <= 0 || h <= 0 || w > 2048 || h > 2048) return {};
    return url + "?w=" + std::to_string(w) + "&h=" + std::to_string(h) +
        "&mode=scale&q=90&format=jpg";
}
void cacheDirectories(const std::string& dir) {
    if (dir != "/data/xcloud4-cache/psbox") return;
    (void)::mkdir("/data/xcloud4-cache", 0700);
    (void)::mkdir(dir.c_str(), 0700);
}
bool diskRead(const std::string& path, uint64_t key, std::string& bytes) {
    FILE* file = std::fopen(path.c_str(), "rb");
    if (!file) return false;
    CacheHeader header{};
    bool ok = std::fread(&header, 1, sizeof(header), file) == sizeof(header) &&
        header.magic == kDiskMagic && header.key == key && header.size > 0 && header.size <= kResponseCap;
    if (ok) {
        bytes.resize(header.size);
        ok = std::fread(bytes.data(), 1, bytes.size(), file) == bytes.size() && std::fgetc(file) == EOF;
    }
    std::fclose(file);
    if (!ok) bytes.clear();
    return ok;
}
void diskWrite(const std::string& path, uint64_t key, const std::string& bytes) {
    if (bytes.empty() || bytes.size() > kResponseCap) return;
    const std::string temp = path.substr(0, path.rfind('/')) + "/write.tmp";
    FILE* file = std::fopen(temp.c_str(), "wb");
    if (!file) return;
    CacheHeader header{key, static_cast<uint32_t>(bytes.size()), kDiskMagic};
    bool ok = std::fwrite(&header, 1, sizeof(header), file) == sizeof(header) &&
        std::fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size() &&
        std::fflush(file) == 0;
    if (std::fclose(file) != 0) ok = false;
    if (ok) ok = std::rename(temp.c_str(), path.c_str()) == 0;
    if (!ok) (void)std::remove(temp.c_str());
}
}

bool decodeImage(const std::string& bytes, Image& out) {
    if (bytes.empty() || bytes.size() > kResponseCap) return false;
    int w = 0, h = 0, channels = 0;
    const auto* raw = reinterpret_cast<const stbi_uc*>(bytes.data());
    int length = static_cast<int>(bytes.size());
    if (!stbi_info_from_memory(raw, length, &w, &h, &channels) || w <= 0 || h <= 0 ||
        w > 4096 || h > 4096 || static_cast<size_t>(w) * h > kDecodedPixels) return false;
    stbi_uc* pixels = stbi_load_from_memory(raw, length, &w, &h, &channels, 4);
    if (!pixels) return false;
    if (w <= 0 || h <= 0 || w > 4096 || h > 4096 ||
        static_cast<size_t>(w) * h > kDecodedPixels) {
        stbi_image_free(pixels);
        return false;
    }
    out.w = w;
    out.h = h;
    out.px.resize(static_cast<size_t>(w) * h);
    std::memcpy(out.px.data(), pixels, out.px.size() * 4);
    stbi_image_free(pixels);
    return true;
}

Image coverResize(const Image& src, int w, int h) {
    Image out;
    if (src.w <= 0 || src.h <= 0 || w <= 0 || h <= 0 || w > 2048 || h > 2048) return out;
    out.w = w;
    out.h = h;
    out.px.resize(static_cast<size_t>(w) * h);
    double scale = std::max(static_cast<double>(w) / src.w, static_cast<double>(h) / src.h);
    int cw = std::min(src.w, static_cast<int>(w / scale + 0.5));
    int ch = std::min(src.h, static_cast<int>(h / scale + 0.5));
    int cx = (src.w - cw) / 2, cy = (src.h - ch) / 2;
    const auto* first = reinterpret_cast<const unsigned char*>(&src.px[static_cast<size_t>(cy) * src.w + cx]);
    stbir_resize_uint8_srgb(first, cw, ch, src.w * 4,
        reinterpret_cast<unsigned char*>(out.px.data()), w, h, w * 4, STBIR_RGBA);
    return out;
}

ImageCache::ImageCache(std::function<void()> onReady, size_t budgetBytes, std::string diskDir,
    size_t diskBudgetBytes)
    : onReady_(std::move(onReady)), budget_(std::min(budgetBytes, size_t{64u << 20})),
      diskDir_(std::move(diskDir)), diskBudget_(diskBudgetBytes) {
    cacheDirectories(diskDir_);
    threads_.resize(1);
    if (!platform::startThread(threads_[0], [this] { loop(); }, 2u << 20)) stop_ = true;
}
ImageCache::~ImageCache() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stop_ = true;
    }
    cv_.notify_all();
    for (auto& thread : threads_) thread.join();
}
std::shared_ptr<const Image> ImageCache::get(const std::string& url, int w, int h) {
    if (!safeImageUrl(url) || w <= 0 || h <= 0 || w > 2048 || h > 2048) return {};
    std::string key = url + "#" + std::to_string(w) + "x" + std::to_string(h);
    std::lock_guard<std::mutex> lock(mutex_);
    Entry& entry = entries_[key];
    entry.lastUse = ++clock_;
    if (!stop_ && !entry.image && !entry.loading &&
        (!entry.failed || (entry.retryAt && platform::nowMs() >= entry.retryAt))) {
        entry.loading = true;
        jobs_.push_front({key, url, w, h});
        cv_.notify_one();
    }
    return entry.image;
}
void ImageCache::evict() {
    while (bytes_ > budget_) {
        auto victim = entries_.end();
        for (auto it = entries_.begin(); it != entries_.end(); ++it)
            if (it->second.image && (victim == entries_.end() || it->second.lastUse < victim->second.lastUse))
                victim = it;
        if (victim == entries_.end()) break;
        bytes_ -= victim->second.image->bytes();
        entries_.erase(victim);
    }
}
std::string ImageCache::diskPath(const std::string& url) const {
    if (diskDir_ != "/data/xcloud4-cache/psbox") return {};
    size_t slots = std::min(static_cast<size_t>(kDiskSlots), diskBudget_ / kResponseCap);
    if (!slots) return {};
    uint64_t hash = hashUrl(url);
    char slot[16];
    std::snprintf(slot, sizeof(slot), "/%02x.img", static_cast<unsigned>(hash % slots));
    return diskDir_ + slot;
}
void ImageCache::trimDisk() { /* Fixed 64 slots, each at most 2 MiB. */ }
void ImageCache::loop() {
    X4Http* http = nullptr;
    for (;;) {
        Job job;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this] { return stop_ || !jobs_.empty(); });
            if (stop_) break;
            job = std::move(jobs_.front());
            jobs_.pop_front();
        }
        std::string url = sizedUrl(job.url, job.w, job.h);
        std::string path = diskPath(url), bytes;
        uint64_t key = hashUrl(url);
        Image decoded;
        bool success = !url.empty() && !path.empty() && diskRead(path, key, bytes) && decodeImage(bytes, decoded);
        int status = 0;
        if (!success && !url.empty()) {
            if (!http) {
                int error = 0;
                http = x4_http_open(&error);
            }
        }
        if (!success && http && !url.empty()) {
            bytes.resize(kResponseCap + 1);
            size_t length = 0;
            int rc = x4_http_store_image(http, url.c_str(), bytes.data(), bytes.size(), &length, &status, nullptr);
            if (rc == 0 && status == 200) {
                bytes.resize(length);
                success = decodeImage(bytes, decoded);
                if (success && !path.empty()) diskWrite(path, key, bytes);
            }
            if (!x4_http_can_retry(http)) {
                x4_http_close(http);
                http = nullptr;
            }
        }
        std::shared_ptr<Image> result;
        if (success) {
            Image sized = decoded.w == job.w && decoded.h == job.h ? std::move(decoded) :
                coverResize(decoded, job.w, job.h);
            if (!sized.px.empty()) result = std::make_shared<Image>(std::move(sized));
        }
        {
            std::lock_guard<std::mutex> lock(mutex_);
            Entry& entry = entries_[job.key];
            entry.loading = false;
            if (result) {
                entry.image = result;
                bytes_ += result->bytes();
                evict();
            } else {
                entry.failed = true;
                if ((status == 0 || status >= 500) && ++entry.failures <= 3)
                    entry.retryAt = platform::nowMs() + 30000u * entry.failures;
                else entry.retryAt = 0;
            }
        }
        if (result && onReady_) onReady_();
    }
    x4_http_close(http);
}
}
