/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include "../core/build_identity.h"

/* Public application capabilities, not a unique console identifier. VideoOut
 * is 1920x1080 for both profiles; the requested stream is chosen separately.
 * Keep the real platform identity rather than claiming to be a Windows app. */
#define X4_XBOX_DEVICE_INFO "{\"appInfo\":{\"env\":{\"clientAppId\":\"Nexo X4\",\"clientAppType\":\"native\"," \
    "\"clientAppVersion\":\"" X4_PRODUCT_VERSION "\",\"httpEnvironment\":\"prod\"}},\"dev\":{\"hw\":{\"make\":\"Sony\"," \
    "\"model\":\"PS4\"},\"os\":{\"name\":\"Orbis\",\"platform\":\"console\"}," \
    "\"displayInfo\":{\"dimensions\":{\"widthInPixels\":1920,\"heightInPixels\":1080}}}}"
