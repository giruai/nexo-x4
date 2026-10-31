/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stddef.h>
#include <stdint.h>

/* Refresh token plaintext: <dir>/session-USER/refresh.txt, restricted to the
 * effective UID, private 0700 directories and a regular 0600 file. No access
 * token, expiry or device code is stored. Never log file contents.
 *
 * Creation writes/fsyncs a complete file in a unique private staging directory,
 * then renames that nonempty directory into place. A completed nonempty session
 * cannot be replaced by a concurrent directory creator. Existing paths of any
 * kind, including empty directories, are refused at creation/import preflight.
 * POSIX rename may replace an empty directory created after that preflight;
 * this is not protection against hostile filesystem changes by the same UID.
 * File fsync is required; crash durability of directory entries is not promised.
 *
 * Renewal replaces refresh.txt atomically inside an existing validated session
 * directory. An exclusive refresh.tmp serializes renewal; an existing temp is
 * left alone and causes failure. Loads support the old refresh-USER.txt format
 * as read-only compatibility when the new session directory is absent. New
 * saves never rewrite a legacy flat file. Clear affects only the supplied own
 * store, validated credential/known temp files and an empty session directory;
 * it never recurses or removes unknown contents/stale staging directories. */
enum { X4_STORE_MISSING = 0, X4_STORE_LOADED = 1, X4_STORE_INVALID = -1, X4_STORE_IO = -2, X4_STORE_EXISTS = -3 };

int x4_session_store_load(const char *dir, int32_t user, const char *client, const char *scope,
    char *token, size_t capacity);
int x4_session_store_save(const char *dir, int32_t user, const char *client, const char *scope,
    const char *token, size_t capacity);
int x4_session_store_clear(const char *dir, int32_t user);

/* Create/check a private 0700 parent; never reads a credential. */
int x4_session_store_prepare(const char *dir);
/* Before any legacy credential read, verify directory rename/nonempty refusal
 * and independent synthetic file contents on this runtime. Cleanup owns only
 * unique directories/files created by this call. No hardlinks/direct syscalls. */
int x4_session_store_probe(const char *dir);
/* Explicit import only. Validates user/client/scope and serializes a completely
 * independent private copy into the new layout. Refuses either own destination
 * layout even if invalid. A runtime synthetic probe must pass before reading
 * the legacy source. Never changes/removes the legacy source. Failure leaves
 * token empty; existing committed credentials are never replaced. */
int x4_session_store_import(const char *legacy_dir, const char *dir, int32_t user,
    const char *client, const char *scope, char *token, size_t capacity);
