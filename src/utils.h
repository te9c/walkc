#ifndef UTILS_H
#define UTILS_H

#define _GNU_SOURCE
#include <sys/types.h>

int sys_pivot_root(const char *new_root, const char *put_old);

int mkdir_if_needed(const char *path, mode_t mode);

#endif