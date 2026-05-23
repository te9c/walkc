#include "metadata.h"
#include "utils.h"

#include <sys/stat.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

char g_runtime_root[PATH_MAX];
int g_runtime_root_len = -1;

int meta_init_root(const char *runtime_root) {
    size_t len;

    if (!runtime_root) {
        errno = EINVAL;
        return -1;
    }

    if (mkdir_if_needed(runtime_root, 0700) < 0) {
        return -1;
    }

    strncpy(g_runtime_root, runtime_root, sizeof(g_runtime_root) - 1);
    g_runtime_root[sizeof(g_runtime_root) - 1] = '\0';

    len = strlen(g_runtime_root);
    if (len == 0) {
        errno = EINVAL;
        return -1;
    }

    g_runtime_root_len = (int)len;
    return 0;
}

int meta_create(const char *id, container_state *st) {
    size_t id_len;
    char id_path[PATH_MAX];

    if (!id || !st) {
        errno = EINVAL;
        return -1;
    }

    if (g_runtime_root_len < 0) {
        errno = EINVAL;
        return -1;
    }

    id_len = strlen(id);
    if (id_len == 0 || id_len >= ID_MAX) {
        errno = EINVAL;
        return -1;
    }

    if ((size_t)g_runtime_root_len + 1 + id_len >= sizeof(id_path)) {
        errno = ENAMETOOLONG;
        return -1;
    }

    memcpy(id_path, g_runtime_root, (size_t)g_runtime_root_len);

    if (g_runtime_root[g_runtime_root_len - 1] != '/') {
        id_path[g_runtime_root_len] = '/';
        memcpy(id_path + g_runtime_root_len + 1, id, id_len + 1);
    } else {
        memcpy(id_path + g_runtime_root_len, id, id_len + 1);
    }

    if (mkdir(id_path, 0700) < 0) {
        return -1;
    }

    memset(st, 0, sizeof(*st));

    strncpy(st->id, id, sizeof(st->id) - 1);
    st->id[sizeof(st->id) - 1] = '\0';

    strncpy(st->oci_version, OCI_VERSION, sizeof(st->oci_version) - 1);
    st->oci_version[sizeof(st->oci_version) - 1] = '\0';

    st->status = CONTAINER_CREATING;

    return 0;
}

const char *container_status_to_string(container_status st) {
    switch (st) {
        case CONTAINER_CREATING: return "creating";
        case CONTAINER_CREATED:  return "created";
        case CONTAINER_RUNNING:  return "running";
        case CONTAINER_STOPPED:  return "stopped";
        default:                 return "unknown";
    }
}

int write_json_file(const char* path, const container_state* state) {
    if (!path || !state) {
        errno = EINVAL;
        return -1;
    }

    FILE* status_file = fopen(path, "w");
    if (!status_file)
        return -1;
    
    int ret = fprintf(status_file,
        "{\n"
        "\"ociVersion\": \"%s\",\n"
        "\"id\": \"%s\",\n"
        "\"status\": \"%s\",\n"
        "\"pid\": %d,\n"
        "\"bundle\": \"%s\"\n"
        "}\n",
        state->oci_version,
        state->id,
        container_status_to_string(state->status),
        (int)state->pid,
        state->bundle
    );
    if (ret < 0) {
        return -1;
    }
    return 0;
}

// int meta_load(const char *id, struct container_state *st) {}
// int meta_save(const char *id, const struct container_state *st) {}
// int meta_set_status(const char *id, const char *status) {}
// int meta_set_pid(const char *id, pid_t pid) {}
// int meta_set_exit_code(const char *id, int code) {}
// int meta_delete(const char *id) {}