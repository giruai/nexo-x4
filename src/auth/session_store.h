/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stddef.h>
#include <stdint.h>

/* The refresh token is plaintext, restricted to a PS4 local-user file (0600).
 * No access token, expiry or device code is stored. Never log file contents. */
enum { X4_STORE_MISSING = 0, X4_STORE_LOADED = 1, X4_STORE_INVALID = -1, X4_STORE_IO = -2 };

int x4_session_store_load(const char *dir, int32_t user, const char *client, const char *scope,
    char *token, size_t capacity);
int x4_session_store_save(const char *dir, int32_t user, const char *client, const char *scope,
    const char *token, size_t capacity);
int x4_session_store_clear(const char *dir, int32_t user);
