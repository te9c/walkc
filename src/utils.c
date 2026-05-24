#include "utils.h"
#include "config.h"

#include <sys/syscall.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <stdio.h>

char _runtime_dir[PATH_MAX];
int _runtime_dir_set = 0;

int sys_pivot_root(const char *new_root, const char *put_old) {
    return syscall(SYS_pivot_root, new_root, put_old);
}

int mkdir_if_needed(const char *path, mode_t mode) {
    if (mkdir(path, mode) == -1 && errno != EEXIST) {
        return -1;
    }
    return 0;
}

const char *runtime_dir() {
    if (_runtime_dir_set)
        return _runtime_dir;

    char *s = getenv("XDG_RUNTIME_DIR");
    if (!s) {
        strcpy(_runtime_dir, FALLBACK_RUNTIME_DIR);
        _runtime_dir_set = 1;
        return _runtime_dir;
    }
    int len = strlen(s);
    if (len + sizeof(RUNTIME_DIR_NAME) + 1 >= PATH_MAX) {
        fprintf(stderr, "runtime path is to big.\n");
        return NULL;
    }
    strcpy(_runtime_dir, s);
    if (_runtime_dir[len - 1] != '/') {
        _runtime_dir[len] = '/';
        _runtime_dir[len + 1] = '\0';
        ++len;
    }
    strcpy(_runtime_dir + len, RUNTIME_DIR_NAME);
    _runtime_dir_set = 1;
    return _runtime_dir;
}