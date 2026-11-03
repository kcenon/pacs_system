/*
 * Fault injection for validation/reliability/fault_probe.py (#1326), loaded into pacs_server with LD_PRELOAD.
 * Every fault applies only to paths under PACS_FAULT_ROOT, so sockets, the index database and logs are untouched.
 *
 *   PACS_FAULT_KILL_BEFORE_RENAME=n  SIGKILL itself at the n-th rename() into the root, before renaming: a complete
 *                                    temporary file is left and nothing is indexed.
 *   PACS_FAULT_KILL_AFTER_RENAME=n   SIGKILL itself right after the n-th rename() into the root: the object's file
 *                                    is in place under its final name and its index rows were never written.
 *   PACS_FAULT_FULL_FLAG=path        while that file exists, write() and writev() to files under the root fail
 *                                    with ENOSPC, as on a full disk.
 *
 * Build: gcc -shared -fPIC -O2 -o faultshim.so faultshim.c -ldl
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <unistd.h>

static atomic_int renames;

static int under_root(const char *path) {
    const char *root = getenv("PACS_FAULT_ROOT");
    return root != NULL && *root != '\0' && path != NULL && strncmp(path, root, strlen(root)) == 0;
}

static long setting(const char *name) {
    const char *value = getenv(name);
    return value != NULL ? strtol(value, NULL, 10) : 0;
}

static int disk_full(void) {
    const char *flag = getenv("PACS_FAULT_FULL_FLAG");
    struct stat status;
    return flag != NULL && stat(flag, &status) == 0;
}

static int file_under_root(int fd) {
    char link[64];
    char path[PATH_MAX];
    snprintf(link, sizeof link, "/proc/self/fd/%d", fd);
    const ssize_t length = readlink(link, path, sizeof path - 1);
    if (length <= 0) {
        return 0;
    }
    path[length] = '\0';
    return under_root(path);
}

int rename(const char *from, const char *to) {
    static int (*real)(const char *, const char *);
    if (real == NULL) {
        real = (int (*)(const char *, const char *))dlsym(RTLD_NEXT, "rename");
    }
    if (!under_root(to)) {
        return real(from, to);
    }
    const int count = atomic_fetch_add(&renames, 1) + 1;
    if (setting("PACS_FAULT_KILL_BEFORE_RENAME") == count) {
        raise(SIGKILL);
    }
    const int result = real(from, to);
    if (setting("PACS_FAULT_KILL_AFTER_RENAME") == count) {
        raise(SIGKILL);
    }
    return result;
}

ssize_t write(int fd, const void *buffer, size_t count) {
    static ssize_t (*real)(int, const void *, size_t);
    if (real == NULL) {
        real = (ssize_t (*)(int, const void *, size_t))dlsym(RTLD_NEXT, "write");
    }
    if (disk_full() && file_under_root(fd)) {
        errno = ENOSPC;
        return -1;
    }
    return real(fd, buffer, count);
}

ssize_t writev(int fd, const struct iovec *vector, int count) {
    static ssize_t (*real)(int, const struct iovec *, int);
    if (real == NULL) {
        real = (ssize_t (*)(int, const struct iovec *, int))dlsym(RTLD_NEXT, "writev");
    }
    if (disk_full() && file_under_root(fd)) {
        errno = ENOSPC;
        return -1;
    }
    return real(fd, vector, count);
}
