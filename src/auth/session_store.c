/* SPDX-License-Identifier: GPL-3.0-only */
#include "session_store.h"
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#ifdef X4_OPENORBIS
#include <orbis/libkernel.h>
#endif

#define X4_STORE_MAGIC "X4RT1\n"
#define X4_STORE_MAX (18u * 1024u)

/* OpenOrbis' bits/stat.h uses a 32-bit mode_t; the PS4 kernel ABI uses
 * 16-bit mode_t followed by 16-bit nlink_t. */
typedef struct {
    uint32_t dev, ino;
    uint16_t mode, nlink;
    uint32_t uid, gid, rdev;
    struct timespec atim, mtim, ctim;
    int64_t size, blocks;
    uint32_t blksize, flags, gen;
    int32_t lspare;
    struct timespec birthtim;
} X4NativeStat;
_Static_assert(offsetof(X4NativeStat, mode) == 8, "PS4 stat mode ABI");
_Static_assert(offsetof(X4NativeStat, uid) == 12, "PS4 stat uid ABI");
_Static_assert(offsetof(X4NativeStat, size) == 72, "PS4 stat size ABI");
_Static_assert(sizeof(X4NativeStat) == 120, "PS4 stat total ABI");
#ifdef X4_OPENORBIS
_Static_assert(sizeof(OrbisKernelStat) >= sizeof(X4NativeStat), "SDK stat buffer size");
_Static_assert(offsetof(OrbisKernelStat, st_uid) == 16, "SDK stat uid offset differs from PS4 ABI");
#endif

typedef struct {
    uint64_t dev, ino;
    uint32_t mode, uid;
    int64_t size;
} X4StoreStat;

static void from_native(const X4NativeStat *native, X4StoreStat *st)
{
    st->dev = native->dev;
    st->ino = native->ino;
    st->mode = native->mode;
    st->uid = native->uid;
    st->size = native->size;
}

/* Fixed stage names only: never print paths, credentials or file contents. */
static int store_failure(const char *stage, int result, int native_rc)
{
    int saved_errno = native_rc == -1 ? errno : 0;
    printf("Nexo X4: store %s result=%d native_rc=%d errno=%d\n",
        stage, result, native_rc, saved_errno);
    return result;
}

static void erase(void *data, size_t size)
{
    volatile unsigned char *p = data;
    while (size--) *p++ = 0;
}

static size_t bounded(const char *s, size_t cap)
{
    size_t n = 0;
    if (!s) return cap;
    while (n < cap && s[n]) ++n;
    return n;
}

static bool safe_field(const char *s, size_t cap, bool spaces)
{
    size_t n = bounded(s, cap);
    if (!n || n == cap) return false;
    for (size_t i = 0; i < n; ++i) {
        unsigned char c = (unsigned char)s[i];
        if (c < (spaces ? 0x20 : 0x21) || c > 0x7e) return false;
    }
    return true;
}

static bool paths(const char *dir, int32_t user, char *file, size_t file_cap,
    char *temp, size_t temp_cap)
{
    if (!dir || user < 0 || !*dir || bounded(dir, 256) == 256) {
        store_failure("paths_input", X4_STORE_INVALID, 0);
        return false;
    }
    int n = snprintf(file, file_cap, "%s/refresh-%d.txt", dir, (int)user);
    int m = snprintf(temp, temp_cap, "%s/refresh-%d.tmp", dir, (int)user);
    bool valid = n > 0 && m > 0 && (size_t)n < file_cap && (size_t)m < temp_cap;
    if (!valid) store_failure("paths_capacity", X4_STORE_INVALID, 0);
    return valid;
}

/* The OpenOrbis libc.a lstat wrapper returns ENOSYS, while sceKernelStat
 * returned synthetic zeroed identity fields on the target. Open without
 * following links and read metadata from the native descriptor instead. */
static int path_stat(const char *path, X4StoreStat *st, const char *stage)
{
#ifdef X4_OPENORBIS
    int fd = sceKernelOpen(path, O_RDONLY | O_NONBLOCK | O_NOFOLLOW, 0);
    if (fd < 0) {
        if ((uint32_t)fd == ORBIS_KERNEL_ERROR_ENOENT) return X4_STORE_MISSING;
        return store_failure(stage, X4_STORE_IO, fd);
    }
    union { OrbisKernelStat sdk; X4NativeStat native; } raw = {0};
    int rc = sceKernelFstat(fd, &raw.sdk);
    int close_rc = sceKernelClose(fd);
    if (close_rc < 0) return store_failure(stage, X4_STORE_IO, close_rc);
    if (rc >= 0) {
        X4NativeStat native;
        memcpy(&native, &raw, sizeof(native));
        from_native(&native, st);
        return X4_STORE_LOADED;
    }
    return store_failure(stage, X4_STORE_IO, rc);
#else
    struct stat host;
    if (lstat(path, &host) == 0) {
        st->dev = host.st_dev; st->ino = host.st_ino;
        st->mode = host.st_mode; st->uid = host.st_uid; st->size = host.st_size;
        return X4_STORE_LOADED;
    }
    return errno == ENOENT ? X4_STORE_MISSING : store_failure(stage, X4_STORE_IO, -1);
#endif
}

static int fd_stat(int fd, X4StoreStat *st, const char *stage)
{
#ifdef X4_OPENORBIS
    union { OrbisKernelStat sdk; X4NativeStat native; } raw = {0};
    int rc = sceKernelFstat(fd, &raw.sdk);
    if (rc < 0) return store_failure(stage, X4_STORE_IO, rc);
    X4NativeStat native;
    memcpy(&native, &raw, sizeof(native));
    from_native(&native, st);
#else
    struct stat host;
    int rc = fstat(fd, &host);
    if (rc != 0) return store_failure(stage, X4_STORE_IO, rc);
    st->dev = host.st_dev; st->ino = host.st_ino;
    st->mode = host.st_mode; st->uid = host.st_uid; st->size = host.st_size;
#endif
    return X4_STORE_LOADED;
}

static int secure_dir(const char *dir, const X4StoreStat *st)
{
#ifdef X4_OPENORBIS
    int fd = sceKernelOpen(dir, O_RDONLY | O_DIRECTORY | O_NOFOLLOW, 0);
    if (fd < 0) {
        return store_failure("dir_open", X4_STORE_INVALID, fd);
    }
    X4StoreStat opened;
    int rc = fd_stat(fd, &opened, "dir_fstat");
    int close_rc = sceKernelClose(fd);
    if (rc != X4_STORE_LOADED || close_rc < 0) {
        printf("Nexo X4: store dir_fstat native_rc=%d close_rc=%d\n", rc, close_rc);
        return rc != X4_STORE_LOADED ? rc : store_failure("dir_close", X4_STORE_IO, close_rc);
    }
    if (!S_ISDIR(opened.mode) || opened.dev != st->dev || opened.ino != st->ino ||
        opened.uid != st->uid || (opened.mode & 0777) != (st->mode & 0777)) {
        printf("Nexo X4: store dir_compare path_mode=%o open_mode=%o path_uid=%u open_uid=%u path_dev=%llu open_dev=%llu path_ino=%llu open_ino=%llu\n",
            (unsigned)st->mode, (unsigned)opened.mode, (unsigned)st->uid,
            (unsigned)opened.uid, (unsigned long long)st->dev,
            (unsigned long long)opened.dev, (unsigned long long)st->ino,
            (unsigned long long)opened.ino);
        return store_failure("dir_compare", X4_STORE_INVALID, 0);
    }
#else
    (void)dir; (void)st;
#endif
    printf("Nexo X4: store dir_verified mode=%o uid=%u real_uid=%u effective_uid=%u\n",
        (unsigned)st->mode, (unsigned)st->uid, (unsigned)getuid(), (unsigned)geteuid());
    return X4_STORE_LOADED;
}

static int ensure_dir(const char *dir)
{
    X4StoreStat st;
    int rc = path_stat(dir, &st, "save_dir");
    if (rc == X4_STORE_MISSING) {
        int mkdir_rc = mkdir(dir, 0700);
        if (mkdir_rc != 0) return store_failure("save_mkdir", X4_STORE_IO, mkdir_rc);
        rc = path_stat(dir, &st, "save_dir_after_mkdir");
    }
    if (rc == X4_STORE_MISSING) return store_failure("save_dir_missing_after_mkdir", X4_STORE_IO, 0);
    if (rc != X4_STORE_LOADED) return rc;
    /* Files are created under the effective UID, which can differ from the
     * real UID in the PS4 sandbox after GoldHEN setup. */
    if (!S_ISDIR(st.mode) || st.uid != geteuid()) {
        printf("Nexo X4: store save_dir_meta mode=%o uid=%u real_uid=%u effective_uid=%u\n",
            (unsigned)st.mode, (unsigned)st.uid, (unsigned)getuid(), (unsigned)geteuid());
        return store_failure("save_dir_meta", X4_STORE_INVALID, 0);
    }
    rc = secure_dir(dir, &st);
    if (rc != X4_STORE_LOADED) return rc;
    if ((st.mode & 0777) != 0700) {
        int chmod_rc = chmod(dir, 0700);
        if (chmod_rc != 0) return store_failure("save_chmod", X4_STORE_IO, chmod_rc);
        rc = path_stat(dir, &st, "save_dir_after_chmod");
        if (rc != X4_STORE_LOADED) return rc;
        return secure_dir(dir, &st);
    }
    return X4_STORE_LOADED;
}

static bool write_all(int fd, const char *data, size_t size)
{
    while (size) {
        ssize_t n = write(fd, data, size);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return false;
        data += n;
        size -= (size_t)n;
    }
    return true;
}

int x4_session_store_save(const char *dir, int32_t user, const char *client, const char *scope,
    const char *token, size_t capacity)
{
    char file[320], temp[320];
    if (!paths(dir, user, file, sizeof(file), temp, sizeof(temp))) return X4_STORE_INVALID;
    if (!safe_field(client, 128, false)) return store_failure("save_client", X4_STORE_INVALID, 0);
    if (!safe_field(scope, 256, true)) return store_failure("save_scope", X4_STORE_INVALID, 0);
    if (!capacity || capacity > 16384) return store_failure("save_capacity", X4_STORE_INVALID, 0);
    if (!safe_field(token, capacity, false)) return store_failure("save_token", X4_STORE_INVALID, 0);
    int rc = ensure_dir(dir);
    if (rc != X4_STORE_LOADED) return rc;
    size_t size = strlen(X4_STORE_MAGIC) + 16 + strlen(client) + strlen(scope) + strlen(token) + 4;
    if (size > X4_STORE_MAX) return store_failure("save_size", X4_STORE_INVALID, 0);
    char *data = malloc(size);
    if (!data) return store_failure("save_alloc", X4_STORE_IO, 0);
    int n = snprintf(data, size, X4_STORE_MAGIC "%d\n%s\n%s\n%s\n", (int)user, client, scope, token);
    if (n < 0 || (size_t)n >= size) {
        erase(data, size); free(data);
        return store_failure("save_format", X4_STORE_INVALID, n);
    }
    (void)unlink(temp); /* an interrupted earlier write, never the committed file */
    int flags = O_WRONLY | O_CREAT | O_EXCL;
#ifdef O_NOFOLLOW
    flags |= O_NOFOLLOW;
#endif
    int fd = open(temp, flags, 0600);
    if (fd < 0) {
        int result = store_failure("save_open", X4_STORE_IO, fd);
        erase(data, size); free(data);
        return result;
    }
    bool ok = true;
    int op = fchmod(fd, 0600);
    if (op != 0) { store_failure("save_fchmod", X4_STORE_IO, op); ok = false; }
    if (ok && !write_all(fd, data, (size_t)n)) {
        store_failure("save_write", X4_STORE_IO, -1);
        ok = false;
    }
    if (ok) {
        op = fsync(fd);
        if (op != 0) { store_failure("save_fsync", X4_STORE_IO, op); ok = false; }
    }
    op = close(fd);
    if (op != 0) { store_failure("save_close", X4_STORE_IO, op); ok = false; }
    if (ok) {
        op = rename(temp, file);
        if (op != 0) { store_failure("save_rename", X4_STORE_IO, op); ok = false; }
    }
    if (!ok) (void)unlink(temp);
    erase(data, size);
    free(data);
    return ok ? X4_STORE_LOADED : X4_STORE_IO;
}

int x4_session_store_load(const char *dir, int32_t user, const char *client, const char *scope,
    char *token, size_t capacity)
{
    if (token && capacity) memset(token, 0, capacity);
    char file[320], temp[320];
    if (!token || !capacity || capacity > 16384 ||
        !paths(dir, user, file, sizeof(file), temp, sizeof(temp)) ||
        !safe_field(client, 128, false) || !safe_field(scope, 256, true)) return X4_STORE_INVALID;
    X4StoreStat st;
    int rc = path_stat(dir, &st, "load_dir");
    if (rc != X4_STORE_LOADED) return rc;
    if (!S_ISDIR(st.mode) || st.uid != geteuid() || (st.mode & 0777) != 0700) {
        printf("Nexo X4: store load dir_meta mode=%o uid=%u real_uid=%u effective_uid=%u\n",
            (unsigned)st.mode, (unsigned)st.uid, (unsigned)getuid(), (unsigned)geteuid());
        return X4_STORE_INVALID;
    }
    rc = secure_dir(dir, &st);
    if (rc != X4_STORE_LOADED) return rc;
    rc = path_stat(file, &st, "load_file");
    if (rc != X4_STORE_LOADED) return rc;
    if (!S_ISREG(st.mode) || st.uid != geteuid() || (st.mode & 0777) != 0600 ||
        st.size <= 0 || st.size > X4_STORE_MAX) {
        printf("Nexo X4: store load_file_meta mode=%o uid=%u real_uid=%u effective_uid=%u size=%lld\n",
            (unsigned)st.mode, (unsigned)st.uid, (unsigned)getuid(), (unsigned)geteuid(),
            (long long)st.size);
        return store_failure("load_file_meta", X4_STORE_INVALID, 0);
    }
    int flags = O_RDONLY;
#ifdef O_NOFOLLOW
    flags |= O_NOFOLLOW;
#endif
    int fd = open(file, flags);
    if (fd < 0) return X4_STORE_IO;
    X4StoreStat opened;
    int stat_rc = fd_stat(fd, &opened, "load_file_fstat");
    if (stat_rc != X4_STORE_LOADED || !S_ISREG(opened.mode) || opened.dev != st.dev ||
        opened.ino != st.ino || opened.uid != st.uid || opened.size != st.size ||
        (opened.mode & 0777) != 0600) {
        int result = stat_rc != X4_STORE_LOADED ? stat_rc :
            store_failure("load_file_compare", X4_STORE_INVALID, 0);
        close(fd);
        return result;
    }
    char *data = malloc((size_t)st.size + 1);
    if (!data) { close(fd); return X4_STORE_IO; }
    size_t got = 0;
    while (got < (size_t)st.size) {
        ssize_t n = read(fd, data + got, (size_t)st.size - got);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) break;
        got += (size_t)n;
    }
    close(fd);
    int result = X4_STORE_INVALID;
    if (got == (size_t)st.size) {
        data[got] = 0;
        char expected[16];
        int m = snprintf(expected, sizeof(expected), "%d", (int)user);
        const char *fields[4] = {X4_STORE_MAGIC, expected, client, scope};
        size_t at = 0;
        bool match = m > 0;
        for (size_t i = 0; match && i < 4; ++i) {
            size_t len = strlen(fields[i]);
            if (i == 0) {
                match = got >= len && !memcmp(data, fields[i], len);
                at = len;
            } else {
                match = at + len < got && !memcmp(data + at, fields[i], len) && data[at + len] == '\n';
                at += len + 1;
            }
        }
        if (match && got > at + 1 && data[got - 1] == '\n') {
            size_t len = got - at - 1;
            if (len < capacity) {
                bool visible = true;
                for (size_t i = 0; i < len; ++i) {
                    unsigned char c = (unsigned char)data[at + i];
                    if (c < 0x21 || c > 0x7e) { visible = false; break; }
                }
                if (visible) {
                    memcpy(token, data + at, len);
                    token[len] = 0;
                    result = X4_STORE_LOADED;
                }
            }
        }
    }
    erase(data, (size_t)st.size + 1);
    free(data);
    return result;
}

int x4_session_store_clear(const char *dir, int32_t user)
{
    char file[320], temp[320];
    if (!paths(dir, user, file, sizeof(file), temp, sizeof(temp))) return X4_STORE_INVALID;
    if (unlink(file) != 0 && errno != ENOENT) return X4_STORE_IO;
    (void)unlink(temp);
    return X4_STORE_LOADED;
}
