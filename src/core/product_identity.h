/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
/* Independent PS4 install identity. Packaging reads these same literals. */
#define NEXO_PRODUCT_TITLE "Nexo X4"
#define NEXO_TITLE_ID "NEXO00001"
#define NEXO_CONTENT_ID "IV0000-NEXO00001_00-NEXOX4APP0000000"
#define NEXO_DATA_ROOT "/data/nexo-x4"
#define NEXO_AUTH_DIR NEXO_DATA_ROOT "/auth"
#define NEXO_UI_DIR NEXO_DATA_ROOT "/ui"
#define NEXO_CACHE_DIR NEXO_DATA_ROOT "/cache"
#define NEXO_PSBOX_CACHE_DIR NEXO_CACHE_DIR "/psbox"
#define NEXO_DIAGNOSTICS_DIR NEXO_DATA_ROOT "/diagnostics"
#define NEXO_DEV_DIR NEXO_DIAGNOSTICS_DIR "/control/"
/* Read only after the user explicitly requests legacy-session import.
 * Never delete or automatically select a shared XCloud4 namespace. */
#define NEXO_LEGACY_AUTH_DIR "/data/xcloud4-auth"
