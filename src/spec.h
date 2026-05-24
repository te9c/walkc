#ifndef SPEC_H
#define SPEC_H

#define _GNU_SOURCE

#include "config.h"

#include <limits.h>
#include <stddef.h>

typedef struct mount_option {
    char destination[PATH_MAX];
    char source[PATH_MAX];
    char type[TYPE_MAX];
    char **options;
    int options_count;
} mount_option;

typedef struct process_option {
    int terminal; // if terminal is attached to the process (?)
    char cwd[PATH_MAX];
    char **argv;
    int argc;
} process_option;

typedef struct config_spec {
    char oci_version[OCI_VERSION_MAX];

    char rootfs_path[PATH_MAX];
    int rootfs_readonly;

    mount_option *mounts;
    int mount_count;

    process_option process;
    int has_process;

    char hostname[PATH_MAX];
} config_spec;

config_spec *alloc_spec(int mount_count);
void free_spec(config_spec *spec);
config_spec *get_default_spec();

char *spec_to_json(const config_spec *spec);
config_spec *spec_from_json(const char *json);

#endif