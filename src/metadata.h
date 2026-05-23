#ifndef METADATA_H
#define METADATA_H

#define _GNU_SOURCE
#include <sys/types.h>
#include <limits.h>

#define ROOT_PATH "/run/walkc"
#define OCI_VERSION "1.0.0"
#define ID_MAX 128 // chars in id including null
#define OCI_VERSION_MAX 32 // chars in oci_version including null
#define TIMESTAMP_MAX 64 // chars in timestamp including null

typedef enum container_status {
    CONTAINER_CREATING = 0,
    CONTAINER_CREATED,
    CONTAINER_RUNNING,
    CONTAINER_STOPPED
} container_status;

typedef struct container_state {
    char oci_version[OCI_VERSION_MAX];
    char id[ID_MAX];
    container_status status;

    pid_t pid; // 0 if its stopped
    char bundle[PATH_MAX];
    // char rootfs[PATH_MAX];

    // int exit_code;
    // int has_exit_code;

    // char created_at[TIMESTAMP_MAX];
    // char started_at[TIMESTAMP_MAX];
    // char finished_at[TIMESTAMP_MAX];

    // char log_path[PATH_MAX];
    // char cgroup_path[PATH_MAX];

    // unsigned int init_once; // guard?
} container_state;

const char* container_status_to_string(container_status st);

int meta_init_root(const char *runtime_root);
int meta_create(const char *id, container_state *st);
// int meta_load(const char *id, struct container_state *st);
// int meta_save(const char *id, const struct container_state *st);
// int meta_set_status(const char *id, const char *status);
// int meta_set_pid(const char *id, pid_t pid);
// int meta_set_exit_code(const char *id, int code);
// int meta_delete(const char *id);

#endif