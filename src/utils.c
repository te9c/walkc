#include "utils.h"

#include <sys/syscall.h>
#include <errno.h>

static int sys_pivot_root(const char *new_root, const char *put_old) {
    return syscall(SYS_pivot_root, new_root, put_old);
}

static int mkdir_if_needed(const char *path, mode_t mode) {
    if (mkdir(path, mode) == -1 && errno != EEXIST) {
        return -1;
    }
    return 0;
}