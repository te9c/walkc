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
#include <signal.h>

char _runtime_dir[PATH_MAX];
int _runtime_dir_set = 0;

int sys_pivot_root(const char *new_root, const char *put_old) {
    return syscall(SYS_pivot_root, new_root, put_old);
}

int mkdir_if_needed(const char *path, mode_t mode) {
    if (mkdir(path, mode) == -1) {
        if (errno == EEXIST) {
            struct stat st;
            if (stat(path, &st) < 0) {
                return -1;
            }
            if ((st.st_mode & S_IFMT) != S_IFDIR) {
                errno = ENOTDIR;
                return -1;
            }
            return 0;
        }
        return -1;
    }
    return 0;
}

const char *runtime_dir(void) {
    if (_runtime_dir_set)
        return _runtime_dir;

    char *s = getenv("XDG_RUNTIME_DIR");
    if (!s) {
        strcpy(_runtime_dir, FALLBACK_RUNTIME_DIR);
        goto create_runtime_dir;
    }
    int len = strlen(s);
    if (len + sizeof(RUNTIME_DIR_NAME) + 1 >= PATH_MAX) {
        errno = ENAMETOOLONG;
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
    if (mkdir_if_needed(_runtime_dir, 0700) < 0) {
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

int copy_bounded(char *dst, size_t dstsz, const char *src) {
    size_t n;

    if (!dst || !src || dstsz == 0) return -1;
    n = strlen(src);
    if (n >= dstsz) return -1;

    memcpy(dst, src, n + 1);
    return 0;
}

int get_string_field_json(json_object *obj, const char *key,
                            char *dst, size_t dstsz, int required) {
    json_object *tmp = NULL;

    if (!json_object_object_get_ex(obj, key, &tmp)) {
        return required ? -1 : 0;
    }
    if (!json_object_is_type(tmp, json_type_string)) return -1;

    return copy_bounded(dst, dstsz, json_object_get_string(tmp));
}

int get_int_field_json(json_object *obj, const char *key,
                         int *out, int required) {
    json_object *tmp = NULL;

    if (!json_object_object_get_ex(obj, key, &tmp)) {
        return required ? -1 : 0;
    }
    if (!json_object_is_type(tmp, json_type_int) &&
        !json_object_is_type(tmp, json_type_boolean)) {
        return -1;
    }

    *out = json_object_get_int(tmp);
    return 0;
}

int sigstr_to_num(const char *sigstr) {
    char *end;
    long v;

    if (sigstr == NULL || *sigstr == '\0')
        return -1;

    errno = 0;
    v = strtol(sigstr, &end, 10);
    if (errno == 0 && *end == '\0' && v > 0 && v < NSIG)
        return (int)v;

    if (strncmp(sigstr, "SIG", 3) == 0)
        sigstr += 3;

#define SIGCASE(name) do { \
    if (strcmp(sigstr, #name) == 0) return SIG##name; \
} while (0)

#ifdef SIGHUP
    SIGCASE(HUP);
#endif
#ifdef SIGINT
    SIGCASE(INT);
#endif
#ifdef SIGQUIT
    SIGCASE(QUIT);
#endif
#ifdef SIGILL
    SIGCASE(ILL);
#endif
#ifdef SIGTRAP
    SIGCASE(TRAP);
#endif
#ifdef SIGABRT
    SIGCASE(ABRT);
#endif
#ifdef SIGBUS
    SIGCASE(BUS);
#endif
#ifdef SIGFPE
    SIGCASE(FPE);
#endif
#ifdef SIGKILL
    SIGCASE(KILL);
#endif
#ifdef SIGUSR1
    SIGCASE(USR1);
#endif
#ifdef SIGSEGV
    SIGCASE(SEGV);
#endif
#ifdef SIGUSR2
    SIGCASE(USR2);
#endif
#ifdef SIGPIPE
    SIGCASE(PIPE);
#endif
#ifdef SIGALRM
    SIGCASE(ALRM);
#endif
#ifdef SIGTERM
    SIGCASE(TERM);
#endif
#ifdef SIGCHLD
    SIGCASE(CHLD);
#endif
#ifdef SIGCONT
    SIGCASE(CONT);
#endif
#ifdef SIGSTOP
    SIGCASE(STOP);
#endif
#ifdef SIGTSTP
    SIGCASE(TSTP);
#endif
#ifdef SIGTTIN
    SIGCASE(TTIN);
#endif
#ifdef SIGTTOU
    SIGCASE(TTOU);
#endif
#ifdef SIGURG
    SIGCASE(URG);
#endif
#ifdef SIGXCPU
    SIGCASE(XCPU);
#endif
#ifdef SIGXFSZ
    SIGCASE(XFSZ);
#endif
#ifdef SIGVTALRM
    SIGCASE(VTALRM);
#endif
#ifdef SIGPROF
    SIGCASE(PROF);
#endif
#ifdef SIGWINCH
    SIGCASE(WINCH);
#endif
#ifdef SIGPOLL
    SIGCASE(POLL);
#endif
#ifdef SIGSYS
    SIGCASE(SYS);
#endif

#undef SIGCASE
    return -1;
}