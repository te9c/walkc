#ifndef UTILS_H
#define UTILS_H

#define _GNU_SOURCE
#include <sys/types.h>
#include <json-c/json.h>
#include <linux/sched.h>
#include <unistd.h>

int sys_pivot_root(const char *new_root, const char *put_old);
int sys_clone3(struct clone_args *cl_args, size_t size);

int mkdir_if_needed(const char *path, mode_t mode);

const char *runtime_dir(void);
const char *get_program_name(void);
int set_program_name(const char *s);

char *read_all_file(const char *path);
int create_file_with_content(const char *path, const char *content);

int copy_bounded(char *dst, size_t dstsz, const char *src);
int get_string_field_json(json_object *obj, const char *key, char *dst, size_t dstsz, int required);
int get_int_field_json(json_object *obj, const char *key, int *out, int required);

int sigstr_to_num(const char *sigstr);

#endif
