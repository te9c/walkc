#define _GNU_SOURCE
#include "container.h"
#include "utils.h"

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

    const char *tmp = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PRETTY);
    if (tmp == NULL) {
        json_object_put(root);
        return NULL;
    }

    char *out = strdup(tmp);
    json_object_put(root);
    return out;
}