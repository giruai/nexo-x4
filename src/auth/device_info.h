/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include "../core/build_identity.h"
#include "../streaming/stream_profile.h"

/* Public application capabilities, not a unique console identifier. VideoOut
 * is 1920x1080 for both profiles; the requested stream is chosen separately.
 * 720p retains this native identity. Experimental 1080 uses the G8 dialect. */
#define X4_XBOX_DEVICE_INFO "{\"appInfo\":{\"env\":{\"clientAppId\":\"Nexo X4\",\"clientAppType\":\"native\"," \
    "\"clientAppVersion\":\"" X4_PRODUCT_VERSION "\",\"httpEnvironment\":\"prod\"}},\"dev\":{\"hw\":{\"make\":\"Sony\"," \
    "\"model\":\"PS4\"},\"os\":{\"name\":\"Orbis\",\"platform\":\"console\"}," \
    "\"displayInfo\":{\"dimensions\":{\"widthInPixels\":1920,\"heightInPixels\":1080}}}}"

/* Adapted from XCloud4 GPL-3.0-only stream_profile.c at
 * 008f4abe4f777fafa4259fe081ebfab065eed2d5. Public browser wire structure
 * researched there from Better xCloud (MIT), commit
 * f8397043f6d2148d2345d508902a38c69cf1ee20, network.ts:316-337.
 * Advertised display dimensions are request metadata, not decode capacity. */
#define X4_XBOX_WEB_WINDOWS_DEVICE_INFO "{\"appInfo\":{\"env\":{\"clientAppId\":\"www.xbox.com\",\"clientAppType\":\"browser\",\"clientAppVersion\":\"26.1.97\",\"clientSdkVersion\":\"10.3.7\",\"httpEnvironment\":\"prod\",\"sdkInstallId\":\"\"}}," \
    "\"dev\":{\"os\":{\"name\":\"windows\",\"ver\":\"22631.2715\",\"platform\":\"desktop\"},\"hw\":{\"make\":\"Microsoft\",\"model\":\"unknown\",\"sdktype\":\"web\"},\"browser\":{\"browserName\":\"chrome\",\"browserVersion\":\"140.0.3485.54\"},\"displayInfo\":{\"dimensions\":{\"widthInPixels\":4096,\"heightInPixels\":2160},\"pixelDensity\":{\"dpiX\":1,\"dpiY\":1}}}}"

static inline const char *x4_xbox_device_info(int profile)
{
    return profile == X4_STREAM_1080 ? X4_XBOX_WEB_WINDOWS_DEVICE_INFO : X4_XBOX_DEVICE_INFO;
}

/* The session worker passes its captured profile. Keep the native dialect,
 * locale and timezone unchanged except for the experimental Windows osName. */
static inline int x4_xbox_play_body(int profile, const char *id, char *out, size_t capacity)
{
    if (!out || !capacity) return -1;
    out[0] = 0;
    if (!id || !x4_stream_profile_valid(profile)) return -1;
    size_t length = 0;
    for (; length <= 128 && id[length]; ++length) {
        unsigned char c = (unsigned char)id[length];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.')) return -1;
    }
    if (!length || length > 128) return -1;
    int n = snprintf(out, capacity,
        "{\"clientSessionId\":\"\",\"titleId\":\"%s\",\"systemUpdateGroup\":\"\",\"settings\":{"
        "\"nanoVersion\":\"V3;WebrtcTransport.dll\",\"enableOptionalDataCollection\":false,"
        "\"enableTextToSpeech\":false,\"highContrast\":0,\"locale\":\"es-MX\",\"useIceConnection\":false,"
        "\"timezoneOffsetMinutes\":360,\"sdkType\":\"web\",\"osName\":\"%s\"},"
        "\"serverId\":\"\",\"fallbackRegionNames\":[]}",
        id, profile == X4_STREAM_1080 ? "windows" : "Orbis");
    if (n >= 0 && (size_t)n < capacity) return n;
    out[0] = 0;
    return -1;
}
