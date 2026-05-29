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
container_status container_status_from_string(const char *s) {
    if (!s)
        return CONTAINER_UNKNOWN;
    if (strcmp(s, "creating") == 0)
        return CONTAINER_CREATING;
    if (strcmp(s, "created") == 0)
        return CONTAINER_CREATED;
    if (strcmp(s, "running") == 0)
        return CONTAINER_RUNNING;
    if (strcmp(s, "stopped") == 0)
        return CONTAINER_STOPPED;
    return CONTAINER_UNKNOWN;
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
        char *spec_json = spec_to_json(cont->spec, 0);
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

    const char *tmp = json_object_to_json_string_ext(root, JSON_C_TO_STRING_NOSLASHESCAPE);
    if (!tmp) {
        json_object_put(root);
        return NULL;
    }

    char *out = strdup(tmp);
    json_object_put(root);
    return out;
}

container *container_from_state_json(const char *json) {
    json_object *root = NULL, *config = NULL;
    container *cont = calloc(1, sizeof(*cont));
    if (!cont) return NULL;

    if (!json) goto fail;

    root = json_tokener_parse(json);
    if (!root) goto fail;
    if (!json_object_is_type(root, json_type_object)) goto fail;

    if (get_string_field_json(root, "id", cont->id, sizeof(cont->id), 1) < 0)
        goto fail;

    char status_string[32];
    if (get_string_field_json(root, "status", status_string, sizeof(status_string), 1) < 0)
        goto fail;
    cont->status = container_status_from_string(status_string);

    if (get_int_field_json(root, "pid", &cont->pid, 1) < 0)
        goto fail;
    if (get_string_field_json(root, "bundle", cont->bundle, sizeof(cont->bundle), 1) < 0)
        goto fail;

    if (!json_object_object_get_ex(root, "config", &config) ||
        !json_object_is_type(config, json_type_object)) {
        goto fail;
    }

    const char *tmp = json_object_to_json_string_ext(config, 0);
    if (!tmp) goto fail;

    cont->spec = spec_from_json(tmp);
    if (!cont->spec) goto fail;

    json_object_put(root);
    return cont;

fail:
    if (root)
        json_object_put(root);
    if (cont) {
        free_spec(cont->spec);
        free(cont);
    }
    return NULL;
}

char *alloc_option_string(char **options, int option_count) {
    if (option_count == 0 || !options)
        return NULL;
    int sz = 0;
    int op_count = 0;
    for (int i = 0; i < option_count; ++i) {
        if (apply_mount_option(NULL, options[i]))
            continue;
    
        sz += strlen(options[i]);
        op_count += 1;
    }
    if (op_count == 0)
        return NULL;
    sz += op_count;
    char *s = malloc(sz * sizeof(char));
    if (!s) {
        return NULL;
    }

    char *cur_pos = s;
    for (int i = 0; i < option_count; ++i) {
        if (apply_mount_option(NULL, options[i]))
            continue;
        cur_pos = stpcpy(cur_pos, options[i]);
        *cur_pos = ',';
        ++cur_pos;
    }
    --cur_pos;
    *cur_pos = '\0';
    return s;
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
        char *data = alloc_option_string(cont->spec->mounts[i].options, cont->spec->mounts[i].option_count);
        int flags = 0;
        for (int j = 0; j < cont->spec->mounts[i].option_count; ++j) {
            apply_mount_option(&flags, cont->spec->mounts[i].options[j]);
        }
        if (mount(cont->spec->mounts[i].source,
                cont->spec->mounts[i].destination,
                cont->spec->mounts[i].type,
                flags, data) < 0) {
            perror("mount");
            free(data);
            return 1;
        }
        free(data);
    }

    if (chdir(cont->spec->process.cwd) < 0) {
        perror("chdir");
        return 1;
    }

    if (sethostname(cont->spec->hostname, strlen(cont->spec->hostname)) < 0) {
        perror("sethostname");
        return 1;
    }

    int argc = cont->spec->process.argument_count;
    char **argv = (char **)calloc(argc + 1, sizeof(char *));
    memcpy(argv, cont->spec->process.arguments, argc * sizeof(char*));
    argv[argc] = NULL;

    execvp(argv[0], argv);
    perror("execvp");
    return 1;
}

int run_container(container *cont) {
    if (!cont || cont->status != CONTAINER_CREATED) {
        return 1;
    }

    const char *rd = runtime_dir();
    if (!rd) {
        perror("runtime_dir");
        return 1;
    }
    if (chdir(rd) < 0) {
        perror("chdir");
        return 1;
    }
    if (chdir(cont->id) < 0) {
        perror("chdir");
        return 1;
    }
    cont->status = CONTAINER_RUNNING;
    char *state_json = container_to_state_json(cont);
    if (!state_json) {
        perror("container_to_state_json");
        return 1;
    }

    cont->pid = clone(
        container_start,
        container_stack + CONTAINER_STACK_SIZE,
        CLONE_NEWNS | CLONE_NEWPID | CLONE_NEWUTS | CLONE_NEWIPC | SIGCHLD,
        cont
    );

    if (cont->pid == -1) {
        free(state_json);
        perror("clone");
        return 1;
    }
    if (create_file_with_content(STATE_FILENAME, state_json) < 0) {
        perror("state.json");
        free(state_json);
        return 1;
    }
    free(state_json);
    state_json = NULL;

    int status;
    if (waitpid(cont->pid, &status, 0) == -1) {
        perror("waitpid");
        return 1;
    }

    cont->status = CONTAINER_STOPPED;
    cont->pid = 0;

    state_json = container_to_state_json(cont);
    if (!state_json) {
        perror("container_to_state_json");
        return 1;
    }
    if (create_file_with_content(STATE_FILENAME, state_json) < 0) {
        perror("state.json");
        free(state_json);
        return 1;
    }
    free(state_json);

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }

    if (WIFSIGNALED(status)) {
        fprintf(stderr, "container killed by signal %d\n", WTERMSIG(status));
    }

    return 1;
}