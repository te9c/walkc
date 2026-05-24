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
        goto create_runtime_dir;
        // _runtime_dir_set = 1;
        // return _runtime_dir;
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

create_runtime_dir:
    if (mkdir(_runtime_dir, 0700) < 0 && errno != EEXIST) {
        return NULL;
    }
    _runtime_dir_set = 1;
    return _runtime_dir;
}

char *read_all_file(const char *path)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) return NULL;

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return NULL;
    }

    long size = ftell(fp);
    if (size < 0) {
        fclose(fp);
        return NULL;
    }

    rewind(fp);

    char *buf = malloc((size_t)size + 1);
    if (!buf) {
        fclose(fp);
        return NULL;
    }

    size_t n = fread(buf, 1, (size_t)size, fp);
    fclose(fp);

    if (n != (size_t)size) {
        free(buf);
        return NULL;
    }

    buf[size] = '\0';
    return buf;
}

int create_file_with_content(const char *path, const char *content) {
    if (path == NULL || content == NULL) {
        return -1;
    }

    FILE *fp = fopen(path, "wb");
    if (fp == NULL) {
        return -1;
    }

    size_t len = strlen(content);
    size_t written = fwrite(content, 1, len, fp);

    if (written != len) {
        fclose(fp);
        return -1;
    }

    if (fclose(fp) != 0) {
        return -1;
    }

    return 0;
}