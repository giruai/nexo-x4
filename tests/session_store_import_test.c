/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef _DARWIN_C_SOURCE
#define _DARWIN_C_SOURCE 1
#endif
#define _POSIX_C_SOURCE 200809L
#include "../src/auth/session_store.h"
#include <assert.h>
#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static const char client[] = "synthetic-client", scope[] = "synthetic scope";
static const char synthetic[] = "SYNTHETIC-NOT-A-CREDENTIAL";
static int commit_race, commit_fail, probe_fail, probe_clobber, sync_fail, refresh_fail;

static void path(char *out, size_t cap, const char *root, const char *suffix)
{
    int n = snprintf(out, cap, "%s/%s", root, suffix);
    assert(n > 0 && (size_t)n < cap);
}

static void write_fixture(const char *file, const char *text)
{
    int fd = open(file, O_WRONLY | O_CREAT | O_EXCL, 0600);
    assert(fd >= 0);
    assert(write(fd, text, strlen(text)) == (ssize_t)strlen(text));
    assert(!close(fd));
}

static void read_fixture(const char *file, char *out, size_t cap)
{
    int fd = open(file, O_RDONLY | O_NOFOLLOW);
    assert(fd >= 0);
    ssize_t n = read(fd, out, cap - 1);
    assert(n >= 0 && (size_t)n < cap - 1);
    out[n] = 0;
    assert(!close(fd));
}

/* Interpose only synthetic fixture operations. renameat invokes the real host
 * primitive; production uses no hardlink or direct syscall. */
int rename(const char *oldpath, const char *newpath)
{
    if (strstr(oldpath, "/.nexo-probe-")) {
        if (probe_fail) { errno = EIO; return -1; }
        if (probe_clobber && !access(newpath, F_OK)) {
            char file[1024];
            path(file, sizeof(file), newpath, "refresh.txt");
            assert(!unlink(file) && !rmdir(newpath));
        }
    } else if (strstr(oldpath, "/.nexo-session-") && strstr(newpath, "/session-42")) {
        if (commit_race) {
            int kind = commit_race;
            commit_race = 0;
            if (kind == 2) write_fixture(newpath, "SYNTHETIC-RACING-FILE");
            else if (kind == 3) assert(!symlink("absent", newpath));
            else {
                assert(!mkdir(newpath, 0700));
                if (kind != 4) {
                    char file[1024];
                    path(file, sizeof(file), newpath, "refresh.txt");
                    write_fixture(file, "SYNTHETIC-RACING-CREDENTIAL");
                }
            }
        }
        if (commit_fail) { errno = EIO; return -1; }
    }
    if (refresh_fail && strstr(oldpath, "/refresh.tmp")) { errno = EIO; return -1; }
    return renameat(AT_FDCWD, oldpath, AT_FDCWD, newpath);
}

int fsync(int fd)
{
    static int (*real_sync)(int);
    if (!real_sync) {
        *(void **)(&real_sync) = dlsym(RTLD_NEXT, "fsync");
        assert(real_sync);
    }
    if (sync_fail) { errno = EIO; return -1; }
    return real_sync(fd);
}

static int child_count(const char *dir)
{
    DIR *d = opendir(dir);
    assert(d);
    int count = 0;
    struct dirent *entry;
    while ((entry = readdir(d)))
        if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) ++count;
    assert(!closedir(d));
    return count;
}

/* Actual old Nexo layout: construct the flat file directly, independently of
 * store_save, which now intentionally writes the new directory layout. */
static void flat_fixture(const char *dir, int filename_user, int payload_user)
{
    assert(!mkdir(dir, 0700));
    char filename[1024], suffix[64], payload[512];
    snprintf(suffix, sizeof(suffix), "refresh-%d.txt", filename_user);
    path(filename, sizeof(filename), dir, suffix);
    snprintf(payload, sizeof(payload), "X4RT1\n%d\n%s\n%s\n%s\n", payload_user, client, scope, synthetic);
    write_fixture(filename, payload);
}

static void expect_legacy(const char *legacy, const struct stat *before, const char *bytes)
{
    char filename[1024], token[128], data[1024];
    struct stat after;
    path(filename, sizeof(filename), legacy, "refresh-42.txt");
    assert(!stat(filename, &after));
    assert(after.st_ino == before->st_ino && after.st_size == before->st_size &&
        after.st_mtime == before->st_mtime && after.st_mode == before->st_mode);
    read_fixture(filename, data, sizeof(data));
    assert(!strcmp(data, bytes));
    assert(x4_session_store_load(legacy, 42, client, scope, token, sizeof(token)) == X4_STORE_LOADED);
    assert(!strcmp(token, synthetic));
}

static int import_session(const char *legacy, const char *own, char *token)
{
    strcpy(token, "stale");
    int rc = x4_session_store_import(legacy, own, 42, client, scope, token, 128);
    if (rc != X4_STORE_LOADED) assert(!token[0]);
    return rc;
}

int main(void)
{
    char root[] = "/tmp/nexo-store-import-XXXXXX";
    assert(mkdtemp(root));
    char legacy[1024], own[1024], session[1024], file[1024], flat[1024], other[1024], token[128];
    path(legacy, sizeof(legacy), root, "legacy");
    path(own, sizeof(own), root, "own");
    path(session, sizeof(session), own, "session-42");
    path(file, sizeof(file), session, "refresh.txt");
    path(flat, sizeof(flat), own, "refresh-42.txt");
    flat_fixture(legacy, 42, 42);
    char legacy_file[1024], legacy_bytes[1024];
    path(legacy_file, sizeof(legacy_file), legacy, "refresh-42.txt");
    read_fixture(legacy_file, legacy_bytes, sizeof(legacy_bytes));
    struct stat before, st;
    assert(!stat(legacy_file, &before));
    assert(x4_session_store_prepare(own) == X4_STORE_LOADED);

    /* A stale staging directory belongs to another invocation: preserve it. */
    char stale[1024], stale_file[1024], suffix[128];
    snprintf(suffix, sizeof(suffix), ".nexo-probe-%ld-0.tmp", (long)getpid());
    path(stale, sizeof(stale), own, suffix);
    assert(!mkdir(stale, 0700));
    path(stale_file, sizeof(stale_file), stale, "refresh.txt");
    write_fixture(stale_file, "SYNTHETIC-STALE");
    assert(x4_session_store_probe(own) == X4_STORE_LOADED);
    assert(child_count(own) == 1 && !access(stale_file, F_OK));
    assert(!unlink(stale_file) && !rmdir(stale));
    assert(!child_count(own));

    probe_fail = 1;
    path(other, sizeof(other), root, "nonexistent-legacy");
    assert(import_session(other, own, token) == X4_STORE_IO); // probe fails before source read
    assert(!child_count(own));
    probe_fail = 0;
    probe_clobber = 1;
    assert(x4_session_store_probe(own) == X4_STORE_IO);
    assert(import_session(legacy, own, token) == X4_STORE_IO);
    probe_clobber = 0;
    assert(!child_count(own));
    expect_legacy(legacy, &before, legacy_bytes);

    assert(import_session(legacy, own, token) == X4_STORE_LOADED);
    assert(!strcmp(token, synthetic));
    assert(!stat(own, &st) && (st.st_mode & 07777) == 0700);
    assert(!stat(session, &st) && S_ISDIR(st.st_mode) && (st.st_mode & 07777) == 0700);
    assert(!stat(file, &st) && S_ISREG(st.st_mode) && (st.st_mode & 07777) == 0600);
    assert(st.st_ino != before.st_ino);
    assert(import_session(legacy, own, token) == X4_STORE_EXISTS);
    assert(x4_session_store_load(own, 42, client, scope, token, sizeof(token)) == X4_STORE_LOADED);
    assert(!strcmp(token, synthetic));
    assert(x4_session_store_save(own, 42, client, scope, "SYNTHETIC-ROTATED", 32) == X4_STORE_LOADED);
    assert(x4_session_store_load(own, 42, client, scope, token, sizeof(token)) == X4_STORE_LOADED);
    assert(!strcmp(token, "SYNTHETIC-ROTATED"));
    sync_fail = 1;
    assert(x4_session_store_save(own, 42, client, scope, synthetic, sizeof(synthetic)) == X4_STORE_IO);
    sync_fail = 0;
    assert(x4_session_store_load(own, 42, client, scope, token, sizeof(token)) == X4_STORE_LOADED);
    assert(!strcmp(token, "SYNTHETIC-ROTATED"));
    refresh_fail = 1;
    assert(x4_session_store_save(own, 42, client, scope, synthetic, sizeof(synthetic)) == X4_STORE_IO);
    refresh_fail = 0;
    assert(x4_session_store_load(own, 42, client, scope, token, sizeof(token)) == X4_STORE_LOADED);
    assert(!strcmp(token, "SYNTHETIC-ROTATED"));
    expect_legacy(legacy, &before, legacy_bytes);
    assert(x4_session_store_clear(own, 42) == X4_STORE_LOADED);
    assert(x4_session_store_load(own, 42, client, scope, token, sizeof(token)) == X4_STORE_MISSING);
    assert(!token[0] && !child_count(own));
    expect_legacy(legacy, &before, legacy_bytes);

    /* Any pre-existing new-layout path, including empty/symlink, is refused. */
    assert(!mkdir(session, 0700));
    assert(import_session(legacy, own, token) == X4_STORE_EXISTS);
    assert(!rmdir(session));
    write_fixture(session, "SYNTHETIC-EXISTING-FILE");
    assert(import_session(legacy, own, token) == X4_STORE_EXISTS);
    assert(!unlink(session));
    assert(!symlink("absent", session));
    assert(import_session(legacy, own, token) == X4_STORE_EXISTS);
    assert(!unlink(session));
    // Existing old-layout paths also refuse import, including dangling links.
    assert(!symlink("absent", flat));
    assert(import_session(legacy, own, token) == X4_STORE_EXISTS);
    assert(!unlink(flat));
    assert(!mkdir(flat, 0700));
    assert(import_session(legacy, own, token) == X4_STORE_EXISTS);
    assert(!rmdir(flat));
    write_fixture(flat, "SYNTHETIC-EXISTING-FLAT");
    assert(import_session(legacy, own, token) == X4_STORE_EXISTS);
    assert(!unlink(flat));

    /* Destination arriving inside rename: completed credentials always win. */
    for (int kind = 1; kind <= 3; ++kind) {
        commit_race = kind;
        assert(import_session(legacy, own, token) == X4_STORE_EXISTS);
        if (kind == 1) {
            char racer[128];
            read_fixture(file, racer, sizeof(racer));
            assert(!strcmp(racer, "SYNTHETIC-RACING-CREDENTIAL"));
            assert(!unlink(file) && !rmdir(session));
        } else assert(!unlink(session));
        assert(!child_count(own));
    }
    // Document the actual POSIX limit: an empty directory racing after the
    // final precheck may be replaced. It contained no committed credential.
    commit_race = 4;
    assert(import_session(legacy, own, token) == X4_STORE_LOADED);
    assert(!strcmp(token, synthetic));
    assert(x4_session_store_clear(own, 42) == X4_STORE_LOADED);
    commit_fail = 1;
    assert(import_session(legacy, own, token) == X4_STORE_IO);
    commit_fail = 0;
    assert(!child_count(own));
    sync_fail = 1;
    assert(x4_session_store_save(own, 42, client, scope, synthetic, sizeof(synthetic)) == X4_STORE_IO);
    sync_fail = 0;
    assert(!child_count(own));

    /* Binding, user and permission failures cannot commit an imported copy. */
    assert(x4_session_store_import(legacy, own, 42, "wrong", scope, token, sizeof(token)) == X4_STORE_INVALID);
    assert(!token[0]);
    assert(x4_session_store_import(legacy, own, 42, client, "wrong", token, sizeof(token)) == X4_STORE_INVALID);
    assert(!token[0]);
    assert(x4_session_store_import(legacy, own, 43, client, scope, token, sizeof(token)) == X4_STORE_MISSING);
    assert(!token[0]);
    path(other, sizeof(other), root, "foreign");
    flat_fixture(other, 42, 43);
    assert(import_session(other, own, token) == X4_STORE_INVALID);
    char foreign_file[1024];
    path(foreign_file, sizeof(foreign_file), other, "refresh-42.txt");
    assert(!unlink(foreign_file) && !rmdir(other));
    assert(!chmod(legacy, 0755));
    assert(import_session(legacy, own, token) == X4_STORE_INVALID);
    assert(!stat(legacy, &st) && (st.st_mode & 07777) == 0755);
    assert(!chmod(legacy, 0700) && !chmod(legacy_file, 0644));
    assert(import_session(legacy, own, token) == X4_STORE_INVALID);
    assert(!stat(legacy_file, &st) && (st.st_mode & 07777) == 0644);
    assert(!chmod(legacy_file, 0600));
    // A source symlink is never followed, even to a private valid fixture.
    path(other, sizeof(other), legacy, "saved-original");
    assert(!rename(legacy_file, other));
    assert(!symlink("saved-original", legacy_file));
    assert(import_session(legacy, own, token) == X4_STORE_INVALID);
    assert(!unlink(legacy_file) && !rename(other, legacy_file));
    expect_legacy(legacy, &before, legacy_bytes);
    assert(!child_count(own));

    /* New-layout sources work too; source inode/data remain independent. */
    char new_source[1024];
    path(new_source, sizeof(new_source), root, "new-source");
    assert(x4_session_store_save(new_source, 42, client, scope, synthetic, sizeof(synthetic)) == X4_STORE_LOADED);
    assert(import_session(new_source, own, token) == X4_STORE_LOADED);
    assert(x4_session_store_clear(own, 42) == X4_STORE_LOADED);
    assert(x4_session_store_load(new_source, 42, client, scope, token, sizeof(token)) == X4_STORE_LOADED);
    assert(!strcmp(token, synthetic));
    assert(x4_session_store_clear(new_source, 42) == X4_STORE_LOADED && !rmdir(new_source));

    /* A normal refresh after reading flat compatibility migrates to new layout
     * without changing that flat file; logout removes both own layouts. */
    write_fixture(flat, legacy_bytes);
    assert(x4_session_store_load(own, 42, client, scope, token, sizeof(token)) == X4_STORE_LOADED);
    assert(!strcmp(token, synthetic));
    assert(x4_session_store_save(own, 42, client, scope, "SYNTHETIC-MIGRATED", 32) == X4_STORE_LOADED);
    char unchanged[1024];
    read_fixture(flat, unchanged, sizeof(unchanged));
    assert(!strcmp(unchanged, legacy_bytes));
    assert(x4_session_store_load(own, 42, client, scope, token, sizeof(token)) == X4_STORE_LOADED);
    assert(!strcmp(token, "SYNTHETIC-MIGRATED"));
    // Unknown contents survive clear; no recursive deletion is attempted.
    path(other, sizeof(other), session, "unrelated");
    write_fixture(other, "SYNTHETIC-UNRELATED");
    assert(x4_session_store_clear(own, 42) == X4_STORE_IO);
    assert(!access(other, F_OK) && !unlink(other));
    assert(x4_session_store_clear(own, 42) == X4_STORE_LOADED);
    assert(!child_count(own));

    /* A stale known temp is not overwritten by renewal and is removed by clear. */
    assert(x4_session_store_save(own, 42, client, scope, synthetic, sizeof(synthetic)) == X4_STORE_LOADED);
    path(other, sizeof(other), session, "refresh.tmp");
    write_fixture(other, "SYNTHETIC-STALE-TEMP");
    assert(x4_session_store_save(own, 42, client, scope, "SYNTHETIC-NEW", 32) == X4_STORE_IO);
    read_fixture(other, unchanged, sizeof(unchanged));
    assert(!strcmp(unchanged, "SYNTHETIC-STALE-TEMP"));
    assert(x4_session_store_load(own, 42, client, scope, token, sizeof(token)) == X4_STORE_LOADED);
    assert(!strcmp(token, synthetic));
    assert(x4_session_store_clear(own, 42) == X4_STORE_LOADED);
    assert(!child_count(own));

    // Clear refuses unsafe credential/temp inodes instead of following links
    // or changing their permissions. The outside marker is never altered.
    assert(x4_session_store_save(own, 42, client, scope, synthetic, sizeof(synthetic)) == X4_STORE_LOADED);
    assert(!chmod(file, 0644));
    assert(x4_session_store_load(own, 42, client, scope, token, sizeof(token)) == X4_STORE_INVALID);
    assert(!token[0]);
    assert(x4_session_store_clear(own, 42) == X4_STORE_INVALID);
    assert(!chmod(file, 0600));
    path(other, sizeof(other), session, "refresh.tmp");
    assert(!symlink("refresh.txt", other));
    assert(x4_session_store_clear(own, 42) == X4_STORE_INVALID);
    assert(!access(file, F_OK) && !unlink(other));
    assert(x4_session_store_clear(own, 42) == X4_STORE_LOADED);

    path(other, sizeof(other), root, "redirect");
    assert(!symlink(legacy, other));
    assert(import_session(legacy, other, token) == X4_STORE_INVALID);
    assert(x4_session_store_clear(other, 42) == X4_STORE_INVALID);
    assert(!unlink(other));
    assert(!chmod(own, 0777));
    assert(x4_session_store_prepare(own) == X4_STORE_LOADED);
    assert(!stat(own, &st) && (st.st_mode & 07777) == 0700);
    path(other, sizeof(other), root, "absent-parent/auth");
    assert(import_session(legacy, other, token) == X4_STORE_IO);
    expect_legacy(legacy, &before, legacy_bytes);

    // Invalid token/payload tests use synthetic bytes only.
    assert(x4_session_store_save(own, 42, client, scope, "bad\ntoken", 32) == X4_STORE_INVALID);
    int fd = open(legacy_file, O_WRONLY | O_TRUNC);
    assert(fd >= 0 && write(fd, "corrupt\n", 8) == 8 && !close(fd));
    assert(import_session(legacy, own, token) == X4_STORE_INVALID);
    assert(!child_count(own));
    assert(x4_session_store_clear(legacy, 42) == X4_STORE_LOADED);
    assert(!rmdir(legacy) && !rmdir(own) && !rmdir(root));
    puts("session_store_import_test: directory commit, probe, legacy compatibility, races and failures passed");
    return 0;
}
