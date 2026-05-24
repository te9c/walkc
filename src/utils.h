#ifndef UTILS_H
#define UTILS_H

#define _GNU_SOURCE
#include <sys/types.h>

int sys_pivot_root(const char *new_root, const char *put_old);

int mkdir_if_needed(const char *path, mode_t mode);

const char *runtime_dir();

char *read_all_file(const char *path);
int create_file_with_content(const char *path, const char *content);

#endif