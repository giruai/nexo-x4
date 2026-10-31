/* SPDX-License-Identifier: GPL-3.0-only */
#include "session_store.h"
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdatomic.h>
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

#ifdef X4_OPENORBIS
static void from_native(const X4NativeStat *native, X4StoreStat *st)
{
    st->dev = native->dev;
    st->ino = native->ino;
    st->mode = native->mode;
    st->uid = native->uid;
    st->size = native->size;
}
#endif

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

typedef struct {
    char user_dir[384], file[416], legacy[384];
} StorePaths;

static bool paths(const char *dir, int32_t user, StorePaths *out)
{
    if (!dir || user < 0 || !*dir || bounded(dir, 256) == 256) return false;
    int a = snprintf(out->user_dir, sizeof(out->user_dir), "%s/session-%d", dir, (int)user);
    int b = snprintf(out->legacy, sizeof(out->legacy), "%s/refresh-%d.txt", dir, (int)user);
    int c = snprintf(out->file, sizeof(out->file), "%s/refresh.txt", out->user_dir);
    return a > 0 && (size_t)a < sizeof(out->user_dir) && b > 0 &&
        (size_t)b < sizeof(out->legacy) && c > 0 && (size_t)c < sizeof(out->file);
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

static int secure_dir(const char *dir, const X4StoreStat *st, bool repair_mode)
{
#ifdef X4_OPENORBIS
    int fd = sceKernelOpen(dir, O_RDONLY | O_DIRECTORY | O_NOFOLLOW, 0);
#else
    int fd = open(dir, O_RDONLY | O_DIRECTORY | O_NOFOLLOW);
#endif
    if (fd < 0) {
        return store_failure("dir_open", X4_STORE_INVALID, fd);
    }
    X4StoreStat opened;
    int rc = fd_stat(fd, &opened, "dir_fstat");
    if (rc != X4_STORE_LOADED) {
        (void)close(fd);
        return rc;
    }
    if (!S_ISDIR(opened.mode) || opened.dev != st->dev || opened.ino != st->ino ||
        opened.uid != st->uid || (opened.mode & 0777) != (st->mode & 0777)) {
        printf("Nexo X4: store dir_compare path_mode=%o open_mode=%o path_uid=%u open_uid=%u path_dev=%llu open_dev=%llu path_ino=%llu open_ino=%llu\n",
            (unsigned)st->mode, (unsigned)opened.mode, (unsigned)st->uid,
            (unsigned)opened.uid, (unsigned long long)st->dev,
            (unsigned long long)opened.dev, (unsigned long long)st->ino,
            (unsigned long long)opened.ino);
        (void)close(fd);
        return store_failure("dir_compare", X4_STORE_INVALID, 0);
    }
    /* Change the verified inode, never a pathname that may have been swapped
     * for a link. Legacy loads pass false and remain strictly read-only. */
    if (repair_mode && (opened.mode & 07777) != 0700) {
        int op = fchmod(fd, 0700);
        if (op != 0) rc = store_failure("dir_fchmod", X4_STORE_IO, op);
        else rc = fd_stat(fd, &opened, "dir_after_fchmod");
    }
    if (rc == X4_STORE_LOADED && (opened.uid != geteuid() ||
        (opened.mode & 07777) != 0700)) rc = X4_STORE_INVALID;
    int close_rc = close(fd);
    if (close_rc != 0) return store_failure("dir_close", X4_STORE_IO, close_rc);
    return rc;
}

static int ensure_dir(const char *dir)
{
    X4StoreStat st;
    int rc = path_stat(dir, &st, "save_dir");
    if (rc == X4_STORE_MISSING) {
        int mkdir_rc = mkdir(dir, 0700);
        if (mkdir_rc != 0 && errno != EEXIST) return store_failure("save_mkdir", X4_STORE_IO, mkdir_rc);
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
    return secure_dir(dir, &st, true);
}

int x4_session_store_prepare(const char *dir)
{
    if (!dir || !*dir || bounded(dir, 256) == 256) return X4_STORE_INVALID;
    return ensure_dir(dir);
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

/* A completed session is a nonempty directory. POSIX rename cannot replace
 * another completed session directory. Existing empty directories are refused
 * by preflight; POSIX cannot exclude an empty directory created in the gap
 * between that check and rename. This is not isolation against hostile writers
 * with this process's effective UID. No credential is ever placed in an empty
 * committed directory, and cooperating racing creators cannot replace a winner. */
static atomic_uint temp_sequence;

typedef struct {
    char dir[448], file[480];
    X4StoreStat identity;
    bool active;
} StagedDir;

static bool same_inode(const X4StoreStat *a, const X4StoreStat *b)
{
    return a->dev == b->dev && a->ino == b->ino;
}

static bool private_file(const X4StoreStat *st)
{
    return S_ISREG(st->mode) && st->uid == geteuid() && (st->mode & 07777) == 0600;
}

static int check_private_dir(const char *dir)
{
    X4StoreStat st;
    int rc = path_stat(dir, &st, "private_dir");
    if (rc != X4_STORE_LOADED) return rc;
    if (!S_ISDIR(st.mode) || st.uid != geteuid() || (st.mode & 07777) != 0700)
        return X4_STORE_INVALID;
    return secure_dir(dir, &st, false);
}

/* Unlike reads, destination preflight must also count dangling symlinks as
 * existing. Native O_NOFOLLOW rejects them with ELOOP rather than opening. */
static int absent_destination(const char *path)
{
#ifdef X4_OPENORBIS
    int fd = sceKernelOpen(path, O_RDONLY | O_NONBLOCK | O_NOFOLLOW, 0);
    if (fd >= 0) {
        int rc = sceKernelClose(fd);
        return rc == 0 ? X4_STORE_EXISTS : store_failure("destination_close", X4_STORE_IO, rc);
    }
    if ((uint32_t)fd == ORBIS_KERNEL_ERROR_ENOENT) return X4_STORE_LOADED;
    if ((uint32_t)fd == ORBIS_KERNEL_ERROR_ELOOP) return X4_STORE_EXISTS;
    return store_failure("destination_open", X4_STORE_IO, fd);
#else
    X4StoreStat st;
    int rc = path_stat(path, &st, "destination");
    if (rc == X4_STORE_LOADED) return X4_STORE_EXISTS;
    return rc == X4_STORE_MISSING ? X4_STORE_LOADED : rc;
#endif
}

static int absent_both(const StorePaths *p)
{
    int rc = absent_destination(p->user_dir);
    return rc == X4_STORE_LOADED ? absent_destination(p->legacy) : rc;
}

static int make_stage(const char *parent, const char *kind, StagedDir *stage)
{
    memset(stage, 0, sizeof(*stage));
    for (unsigned attempt = 0; attempt < 32; ++attempt) {
        unsigned sequence = atomic_fetch_add_explicit(&temp_sequence, 1, memory_order_relaxed);
        int n = snprintf(stage->dir, sizeof(stage->dir), "%s/.nexo-%s-%ld-%u.tmp", parent,
            kind, (long)getpid(), sequence);
        if (n <= 0 || (size_t)n >= sizeof(stage->dir)) return X4_STORE_INVALID;
        if (mkdir(stage->dir, 0700) == 0) {
            int rc = path_stat(stage->dir, &stage->identity, "stage_stat");
            if (rc != X4_STORE_LOADED) return rc;
            stage->active = true;
            int m = snprintf(stage->file, sizeof(stage->file), "%s/refresh.txt", stage->dir);
            if (m <= 0 || (size_t)m >= sizeof(stage->file)) return X4_STORE_INVALID;
            return check_private_dir(stage->dir);
        }
        if (errno != EEXIST) return store_failure("stage_mkdir", X4_STORE_IO, -1);
    }
    return X4_STORE_IO;
}

/* Never remove a stale directory from another invocation, or recurse into
 * unexpected contents. Cleanup owns exactly this inode and refresh.txt. */
static int cleanup_stage(StagedDir *stage)
{
    if (!stage->active) return X4_STORE_LOADED;
    X4StoreStat st;
    int rc = path_stat(stage->dir, &st, "stage_cleanup");
    if (rc != X4_STORE_LOADED || !same_inode(&st, &stage->identity) ||
        !S_ISDIR(st.mode) || st.uid != geteuid()) return X4_STORE_IO;
    if (unlink(stage->file) != 0 && errno != ENOENT) return X4_STORE_IO;
    if (rmdir(stage->dir) != 0) return X4_STORE_IO;
    stage->active = false;
    return X4_STORE_LOADED;
}

static int write_private_file(const char *file, const char *data, size_t size)
{
    int fd = open(file, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600);
    if (fd < 0) return store_failure("write_open", X4_STORE_IO, -1);
    int rc = X4_STORE_LOADED;
    X4StoreStat st;
    if (fchmod(fd, 0600) != 0 || fd_stat(fd, &st, "write_fstat") != X4_STORE_LOADED ||
        !private_file(&st) || !write_all(fd, data, size) || fsync(fd) != 0)
        rc = store_failure("write_sync", X4_STORE_IO, -1);
    if (close(fd) != 0) rc = store_failure("write_close", X4_STORE_IO, -1);
    return rc;
}

static int commit_directory(StagedDir *stage, const char *destination)
{
    int rc = absent_destination(destination);
    if (rc != X4_STORE_LOADED) return rc;
    if (rename(stage->dir, destination) != 0) {
        int error = errno;
        if (error == EEXIST || error == ENOTEMPTY || error == ENOTDIR || error == EISDIR)
            return X4_STORE_EXISTS;
        return store_failure("directory_commit", X4_STORE_IO, -1);
    }
    stage->active = false;
    return X4_STORE_LOADED;
}

static const char probe_original[] = "NEXO-SYNTHETIC-PROBE-A\n";
static const char probe_modified[] = "NEXO-SYNTHETIC-PROBE-B\n";

static bool probe_matches(const char *file, const X4StoreStat *expected)
{
    X4StoreStat st;
    if (path_stat(file, &st, "probe_file") != X4_STORE_LOADED || !private_file(&st) ||
        !same_inode(&st, expected) || st.size != (int64_t)(sizeof(probe_original) - 1)) return false;
    int fd = open(file, O_RDONLY | O_NOFOLLOW | O_NONBLOCK);
    if (fd < 0) return false;
    char data[sizeof(probe_original)];
    size_t got = 0;
    while (got < sizeof(data)) {
        ssize_t n = read(fd, data + got, sizeof(data) - got);
        if (n < 0 && errno == EINTR) continue;
        if (n < 0) { (void)close(fd); return false; }
        if (!n) break;
        got += (size_t)n;
    }
    int closed = close(fd);
    return closed == 0 && got == sizeof(probe_original) - 1 && !memcmp(data, probe_original, got);
}

int x4_session_store_probe(const char *dir)
{
    int rc = x4_session_store_prepare(dir);
    if (rc != X4_STORE_LOADED) return rc;
    StagedDir arena = {0}, source = {0}, copy = {0};
    unsigned probe_stage = 1;
    rc = make_stage(dir, "probe", &arena);
    if (rc != X4_STORE_LOADED) goto cleanup;
    rc = make_stage(arena.dir, "source", &source);
    if (rc != X4_STORE_LOADED) goto cleanup;
    rc = make_stage(arena.dir, "copy", &copy);
    if (rc != X4_STORE_LOADED) goto cleanup;
    rc = X4_STORE_IO;
    probe_stage = 2;
    if (write_private_file(source.file, probe_original, sizeof(probe_original) - 1) != X4_STORE_LOADED ||
        write_private_file(copy.file, probe_original, sizeof(probe_original) - 1) != X4_STORE_LOADED) goto cleanup;
    X4StoreStat original, copied;
    if (path_stat(source.file, &original, "probe_source") != X4_STORE_LOADED ||
        path_stat(copy.file, &copied, "probe_copy") != X4_STORE_LOADED || same_inode(&original, &copied)) goto cleanup;
    char destination[448], destination_file[480];
    int n = snprintf(destination, sizeof(destination), "%s/committed", arena.dir);
    int m = snprintf(destination_file, sizeof(destination_file), "%s/refresh.txt", destination);
    if (n <= 0 || (size_t)n >= sizeof(destination) || m <= 0 || (size_t)m >= sizeof(destination_file)) goto cleanup;
    probe_stage = 3;
    if (rename(copy.dir, destination) != 0) goto cleanup;
    strcpy(copy.dir, destination);
    strcpy(copy.file, destination_file);
    if (!probe_matches(copy.file, &copied)) goto cleanup;
    probe_stage = 4;
    int fd = open(source.file, O_WRONLY | O_TRUNC | O_NOFOLLOW);
    if (fd < 0) goto cleanup;
    bool changed = write_all(fd, probe_modified, sizeof(probe_modified) - 1) && fsync(fd) == 0;
    if (close(fd) != 0) changed = false;
    if (!changed || !probe_matches(copy.file, &copied)) goto cleanup;
    probe_stage = 5;
    /* Test the runtime primitive itself, bypassing our preflight on purpose. */
    int renamed = rename(source.dir, copy.dir);
    int rename_error = errno;
    if (renamed == 0) {
        strcpy(source.dir, copy.dir);
        strcpy(source.file, copy.file);
        copy.active = false;
        goto cleanup;
    }
    if ((rename_error != EEXIST && rename_error != ENOTEMPTY) || !probe_matches(copy.file, &copied)) goto cleanup;
    rc = X4_STORE_LOADED;
cleanup:
    if (rc != X4_STORE_LOADED) printf("Nexo X4: store probe stage=%u errno=%d\n", probe_stage, errno);
    if (cleanup_stage(&source) != X4_STORE_LOADED) rc = X4_STORE_IO;
    if (cleanup_stage(&copy) != X4_STORE_LOADED) rc = X4_STORE_IO;
    if (cleanup_stage(&arena) != X4_STORE_LOADED) rc = X4_STORE_IO;
    return rc;
}

static int store_write(const char *dir, int32_t user, const char *client, const char *scope,
    const char *token, size_t capacity, bool no_replace)
{
    StorePaths p;
    if (!paths(dir, user, &p) || !safe_field(client, 128, false) || !safe_field(scope, 256, true) ||
        !capacity || capacity > 16384 || !safe_field(token, capacity, false)) return X4_STORE_INVALID;
    int rc = ensure_dir(dir);
    if (rc != X4_STORE_LOADED) return rc;
    if (no_replace) {
        rc = absent_both(&p);
        if (rc != X4_STORE_LOADED) return rc;
    }
    X4StoreStat existing;
    rc = path_stat(p.user_dir, &existing, "save_session");
    bool updating = rc == X4_STORE_LOADED;
    if (rc != X4_STORE_LOADED && rc != X4_STORE_MISSING) return rc;
    if (updating && no_replace) return X4_STORE_EXISTS;
    if (updating) {
        rc = check_private_dir(p.user_dir);
        if (rc != X4_STORE_LOADED) return rc;
        rc = path_stat(p.file, &existing, "save_existing");
        if (rc != X4_STORE_LOADED || !private_file(&existing)) return X4_STORE_INVALID;
    }
    size_t size = strlen(X4_STORE_MAGIC) + 16 + strlen(client) + strlen(scope) + strlen(token) + 4;
    if (size > X4_STORE_MAX) return X4_STORE_INVALID;
    char *data = malloc(size);
    if (!data) return X4_STORE_IO;
    int n = snprintf(data, size, X4_STORE_MAGIC "%d\n%s\n%s\n%s\n", (int)user, client, scope, token);
    if (n < 0 || (size_t)n >= size) { erase(data, size); free(data); return X4_STORE_INVALID; }
    StagedDir stage = {0};
    char temp[480] = {0};
    bool own_temp = false;
    if (updating) {
        int m = snprintf(temp, sizeof(temp), "%s/refresh.tmp", p.user_dir);
        if (m <= 0 || (size_t)m >= sizeof(temp)) rc = X4_STORE_INVALID;
        else {
            /* Never unlink another writer's in-progress or stale temp file. */
            int fd = open(temp, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600);
            if (fd < 0) rc = X4_STORE_IO;
            else {
                own_temp = true;
                X4StoreStat temp_stat;
                bool ok = fchmod(fd, 0600) == 0 &&
                    fd_stat(fd, &temp_stat, "refresh_fstat") == X4_STORE_LOADED && private_file(&temp_stat) &&
                    write_all(fd, data, (size_t)n) && fsync(fd) == 0;
                if (close(fd) != 0) ok = false;
                rc = ok ? X4_STORE_LOADED : X4_STORE_IO;
                if (ok && rename(temp, p.file) != 0) rc = store_failure("refresh_commit", X4_STORE_IO, -1);
            }
        }
    } else {
        rc = make_stage(dir, "session", &stage);
        if (rc == X4_STORE_LOADED) rc = write_private_file(stage.file, data, (size_t)n);
        if (rc == X4_STORE_LOADED && no_replace) rc = absent_both(&p);
        if (rc == X4_STORE_LOADED) rc = commit_directory(&stage, p.user_dir);
    }
    if (own_temp && rc != X4_STORE_LOADED && unlink(temp) != 0 && errno != ENOENT) rc = X4_STORE_IO;
    if (cleanup_stage(&stage) != X4_STORE_LOADED) rc = X4_STORE_IO;
    erase(data, size);
    free(data);
    return rc;
}

int x4_session_store_save(const char *dir, int32_t user, const char *client, const char *scope,
    const char *token, size_t capacity)
{
    return store_write(dir, user, client, scope, token, capacity, false);
}

static int read_token_file(const char *file, int32_t user, const char *client, const char *scope,
    char *token, size_t capacity)
{
    X4StoreStat st;
    int rc;
    rc = path_stat(file, &st, "load_file");
    if (rc != X4_STORE_LOADED) return rc;
    if (!S_ISREG(st.mode) || st.uid != geteuid() || (st.mode & 07777) != 0600 ||
        st.size <= 0 || st.size > X4_STORE_MAX) {
        printf("Nexo X4: store load_file_meta mode=%o uid=%u real_uid=%u effective_uid=%u size=%lld\n",
            (unsigned)st.mode, (unsigned)st.uid, (unsigned)getuid(), (unsigned)geteuid(),
            (long long)st.size);
        return store_failure("load_file_meta", X4_STORE_INVALID, 0);
    }
    int flags = O_RDONLY | O_NONBLOCK;
#ifdef O_NOFOLLOW
    flags |= O_NOFOLLOW;
#endif
    int fd = open(file, flags);
    if (fd < 0) return X4_STORE_IO;
    X4StoreStat opened;
    int stat_rc = fd_stat(fd, &opened, "load_file_fstat");
    if (stat_rc != X4_STORE_LOADED || !S_ISREG(opened.mode) || opened.dev != st.dev ||
        opened.ino != st.ino || opened.uid != st.uid || opened.size != st.size ||
        (opened.mode & 07777) != 0600) {
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
    int close_rc = close(fd);
    int result = X4_STORE_INVALID;
    if (close_rc != 0) { erase(data, (size_t)st.size + 1); free(data); return X4_STORE_IO; }
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


int x4_session_store_load(const char *dir, int32_t user, const char *client, const char *scope,
    char *token, size_t capacity)
{
    if (token && capacity) memset(token, 0, capacity);
    StorePaths p;
    if (!token || !capacity || capacity > 16384 || !paths(dir, user, &p) ||
        !safe_field(client, 128, false) || !safe_field(scope, 256, true)) return X4_STORE_INVALID;
    int rc = check_private_dir(dir);
    if (rc != X4_STORE_LOADED) return rc;
    rc = check_private_dir(p.user_dir);
    if (rc == X4_STORE_MISSING) return read_token_file(p.legacy, user, client, scope, token, capacity);
    if (rc != X4_STORE_LOADED) return rc;
    return read_token_file(p.file, user, client, scope, token, capacity);
}

int x4_session_store_import(const char *legacy_dir, const char *dir, int32_t user,
    const char *client, const char *scope, char *token, size_t capacity)
{
    if (token && capacity) memset(token, 0, capacity);
    StorePaths p;
    if (!token || !capacity || capacity > 16384 || !legacy_dir || !dir || !strcmp(legacy_dir, dir) ||
        !paths(dir, user, &p) || !safe_field(client, 128, false) || !safe_field(scope, 256, true)) return X4_STORE_INVALID;
    int rc = ensure_dir(dir);
    if (rc == X4_STORE_LOADED) rc = absent_both(&p);
    if (rc != X4_STORE_LOADED) return rc;
    rc = x4_session_store_probe(dir);
    printf("Nexo X4: import probe=%d\n", rc);
    if (rc != X4_STORE_LOADED) return rc;
    char *candidate = calloc(1, capacity);
    if (!candidate) return X4_STORE_IO;
    rc = x4_session_store_load(legacy_dir, user, client, scope, candidate, capacity);
    if (rc == X4_STORE_LOADED) {
        rc = store_write(dir, user, client, scope, candidate, capacity, true);
        if (rc == X4_STORE_LOADED) memcpy(token, candidate, capacity);
    }
    erase(candidate, capacity);
    free(candidate);
    return rc;
}

static int remove_private_file(const char *file)
{
    X4StoreStat st;
    int rc = path_stat(file, &st, "clear_file");
    if (rc == X4_STORE_MISSING) return X4_STORE_LOADED;
    if (rc != X4_STORE_LOADED) return rc;
    if (!private_file(&st)) return X4_STORE_INVALID;
    return unlink(file) == 0 ? X4_STORE_LOADED : X4_STORE_IO;
}

int x4_session_store_clear(const char *dir, int32_t user)
{
    StorePaths p;
    if (!paths(dir, user, &p)) return X4_STORE_INVALID;
    int rc = check_private_dir(dir);
    if (rc == X4_STORE_MISSING) return X4_STORE_LOADED;
    if (rc != X4_STORE_LOADED) return rc;
    rc = check_private_dir(p.user_dir);
    if (rc == X4_STORE_LOADED) {
        char temp[480];
        int n = snprintf(temp, sizeof(temp), "%s/refresh.tmp", p.user_dir);
        if (n <= 0 || (size_t)n >= sizeof(temp)) return X4_STORE_INVALID;
        rc = remove_private_file(temp);
        if (rc == X4_STORE_LOADED) rc = remove_private_file(p.file);
        if (rc != X4_STORE_LOADED) return rc;
        if (rmdir(p.user_dir) != 0) return X4_STORE_IO;
    } else if (rc != X4_STORE_MISSING) return rc;
    rc = remove_private_file(p.legacy);
    if (rc != X4_STORE_LOADED) return rc;
    char legacy_temp[384];
    int n = snprintf(legacy_temp, sizeof(legacy_temp), "%s/refresh-%d.tmp", dir, (int)user);
    if (n <= 0 || (size_t)n >= sizeof(legacy_temp)) return X4_STORE_INVALID;
    return remove_private_file(legacy_temp);
}
