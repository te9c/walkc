#define _GNU_SOURCE
#include "container.h"
#include "utils.h"
#include "config.h"

#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <json-c/json.h>

const char *container_status_to_string(container_status st) {
    switch (st) {
        case CONTAINER_CREATING: return "creating";
        case CONTAINER_CREATED:  return "created";
        case CONTAINER_RUNNING:  return "running";
        case CONTAINER_STOPPED:  return "stopped";
        default:                 return "unknown";
    }
}

char *container_to_state_json(container *cont) {
    if (cont == NULL) {
        return NULL;
    }

    json_object *root = json_object_new_object();
    if (root == NULL) {
        return NULL;
    }

    json_object_object_add(root, "ociVersion",
        json_object_new_string(
            (cont->spec != NULL) ? cont->spec->oci_version : ""
        ));

    json_object_object_add(root, "id",
        json_object_new_string(cont->id));

    json_object_object_add(root, "status",
        json_object_new_string(container_status_to_string(cont->status)));

    json_object_object_add(root, "pid",
        json_object_new_int64((int64_t)cont->pid));

    json_object_object_add(root, "bundle",
        json_object_new_string(cont->bundle));

    json_object_object_add(root, "annotations",
        json_object_new_object());

    if (cont->spec != NULL) {
        char *spec_json = spec_to_json(cont->spec);
        if (spec_json != NULL) {
            json_object *config_obj = json_tokener_parse(spec_json);

            if (config_obj != NULL) {
                json_object_object_add(root, "config", config_obj);
            } else {
                json_object_object_add(root, "config", NULL);
            }

            free(spec_json);
        } else {
            json_object_object_add(root, "config", NULL);
        }
    } else {
        json_object_object_add(root, "config", NULL);
    }

    const char *tmp = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_NOSLASHESCAPE);
    if (tmp == NULL) {
        json_object_put(root);
        return NULL;
    }

    char *out = strdup(tmp);
    json_object_put(root);
    return out;
}

static char container_stack[CONTAINER_STACK_SIZE];

static int container_start(void *arg) {
    container *cont = (container *)arg;
    cont->pid = getpid();
    char old_root[PATH_MAX];

    if (mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL) == -1) {
        perror("mount MS_PRIVATE");
        return 1;
    }

    if (mount(cont->spec->rootfs_path, cont->spec->rootfs_path, NULL,
        MS_BIND | MS_REC | (MS_RDONLY * cont->spec->rootfs_readonly), NULL) == -1) {
        perror("bind-mount rootfs");
        return 1;
    }

    if (snprintf(old_root, sizeof(old_root), "%s/.old_root", cont->spec->rootfs_path) >= (int)sizeof(old_root)) {
        fprintf(stderr, "rootfs path too long\n");
        return 1;
    }

    if (mkdir_if_needed(old_root, 0755) == -1) {
        perror("mkdir .old_root");
        return 1;
    }

    if (sys_pivot_root(cont->spec->rootfs_path, old_root) == -1) {
        perror("pivot_root");
        return 1;
    }

    if (chdir("/") == -1) {
        perror("chdir");
        return 1;
    }

    if (umount2("/.old_root", MNT_DETACH) == -1) {
        perror("umount old root");
        return 1;
    }

    if (rmdir("/.old_root") == -1) {
        perror("rmdir old root");
        return 1;
    }

    for (int i = 0; i < cont->spec->mount_count; ++i) {
        if (mkdir_if_needed(cont->spec->mounts[i].destination, 0555) < 0) {
            perror("mkdir");
            return 1;
        }
        if (mount(cont->spec->mounts[i].source,
                cont->spec->mounts[i].destination,
                cont->spec->mounts[i].type,
                NULL, NULL) < 0) {
            perror("mount");
            return 1;
        }
    }

    if (chdir(cont->spec->process.cwd) < 0) {
        perror("chdir");
        return 1;
    }
    execvp(cont->spec->process.argv[0], cont->spec->process.argv);
    perror("execvp");
    return 1;
}

int run_container(container *cont) {
    if (!cont || cont->status != CONTAINER_CREATED) {
        return 1;
    }

    cont->pid = clone(
        container_start,
        container_stack + CONTAINER_STACK_SIZE,
        CLONE_NEWNS | CLONE_NEWPID | CLONE_NEWUTS | CLONE_NEWIPC | SIGCHLD,
        cont
    );
}