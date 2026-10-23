/* SPDX-License-Identifier: GPL-3.0-only */
/* Frozen native sources plus independent, explicit experiment modes. */
#pragma once
#define X4_PRODUCT_VERSION "0.1.10"
#define X4_SOURCE_ID "nexo-x4-v0.1.10-directwb"
#if X4_RECEIVER_NACK
#define X4_NACK_ID "-n1"
#else
#define X4_NACK_ID "-n0"
#endif
#if X4_NACK_GRACE_MS == 45
#define X4_GRACE_ID "-g45"
#else
#define X4_GRACE_ID "-g0"
#endif
#if X4_RUMBLE
#define X4_RUMBLE_ID "-r1"
#else
#define X4_RUMBLE_ID "-r0"
#endif
#if X4_DEV_CONTROL
#define X4_DEV_ID "-dev1"
#else
#define X4_DEV_ID "-dev0"
#endif
#define X4_BUILD_ID X4_SOURCE_ID X4_NACK_ID X4_GRACE_ID X4_RUMBLE_ID X4_DEV_ID
