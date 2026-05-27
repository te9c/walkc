#include "spec.h"
#include "utils.h"

#include <string.h>
#include <stdlib.h>
#include <json-c/json.h>


config_spec* alloc_spec(int mount_count) {
    if (mount_count < 0)
        return NULL;
    config_spec* spec = malloc(sizeof(config_spec));

    if (!spec)
        return NULL;
    
    memset(spec, 0, sizeof(config_spec));
    spec->mount_count = mount_count;
    spec->mounts = malloc(sizeof(mount_option) * mount_count);
    if (!spec->mounts) {
        free(spec);
        return NULL;
    }
    memset(spec->mounts, 0, mount_count * sizeof(mount_option));

    return spec;
}

void free_spec(config_spec* spec) {
    if (!spec)
        return;
    
    for (int i = 0; i < spec->mount_count; ++i) {
        for (int j = 0; j < spec->mounts[i].options_count; ++i) {
            free(spec->mounts[i].options[j]);
        }
        free(spec->mounts[i].options);
    }
    free(spec->mounts);

    if (spec->has_process) {
        for (int i = 0; i < spec->process.argc; ++i) {
            free(spec->process.argv[i]);
        }
        free(spec->process.argv);
    }
    free(spec);
}

config_spec *get_default_spec(void) {
    config_spec *spec = alloc_spec(1);
    if (!spec)
        return NULL;

    strcpy(spec->oci_version, OCI_VERSION);
    strcpy(spec->rootfs_path, DEFAULT_ROOTFS_PATH);
    spec->rootfs_readonly = 0;

    strcpy(spec->mounts[0].destination, "/proc");
    strcpy(spec->mounts[0].type, "proc");
    strcpy(spec->mounts[0].source, "proc");

    strcpy(spec->process.cwd, "/");
    spec->process.argv = malloc(sizeof(char*));
    if (!spec->process.argv) {
        free_spec(spec);
        return NULL;
    }
    spec->process.argv[0] = malloc(sizeof(DEFUALT_RUN_PROGRAM));
    if (!spec->process.argv[0]) {
        free(spec->process.argv);
        free_spec(spec);
        return NULL;
    }
    strcpy(spec->process.argv[0], DEFUALT_RUN_PROGRAM);
    spec->process.argc = 1;
    spec->has_process = 1;

    strcpy(spec->hostname, DEFAULT_HOSTNAME);
    return spec;
}

char *spec_to_json(const config_spec *spec) {
    json_object *root, *rootfs, *mounts, *proc;
    const char *tmp;
    char *out;
    int i, j;

    if (!spec) return NULL;

    root = json_object_new_object();
    if (!root) return NULL;

    json_object_object_add(root, "ociVersion",
        json_object_new_string(spec->oci_version));
    json_object_object_add(root, "hostname",
        json_object_new_string(spec->hostname));

    rootfs = json_object_new_object();
    json_object_object_add(rootfs, "path",
        json_object_new_string(spec->rootfs_path));
    json_object_object_add(rootfs, "readonly",
        json_object_new_boolean(spec->rootfs_readonly));
    json_object_object_add(root, "root", rootfs);

    mounts = json_object_new_array();
    for (i = 0; i < spec->mount_count; ++i) {
        json_object *jm = json_object_new_object();
        json_object *jopts = json_object_new_array();

        json_object_object_add(jm, "destination",
            json_object_new_string(spec->mounts[i].destination));
        json_object_object_add(jm, "source",
            json_object_new_string(spec->mounts[i].source));
        json_object_object_add(jm, "type",
            json_object_new_string(spec->mounts[i].type));

        for (j = 0; j < spec->mounts[i].options_count; ++j) {
            json_object_array_add(jopts,
                json_object_new_string(spec->mounts[i].options[j]));
        }

        json_object_object_add(jm, "options", jopts);
        json_object_array_add(mounts, jm);
    }
    json_object_object_add(root, "mounts", mounts);

    if (spec->has_process) {
        json_object *jargv = json_object_new_array();

        proc = json_object_new_object();
        json_object_object_add(proc, "terminal",
            json_object_new_boolean(spec->process.terminal));
        json_object_object_add(proc, "cwd",
            json_object_new_string(spec->process.cwd));

        for (i = 0; i < spec->process.argc; ++i) {
            json_object_array_add(jargv,
                json_object_new_string(spec->process.argv[i]));
        }

        json_object_object_add(proc, "args", jargv);
        json_object_object_add(root, "process", proc);
    }

    tmp = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_NOSLASHESCAPE);
    out = tmp ? strdup(tmp) : NULL;

    json_object_put(root);
    return out;
}

config_spec *spec_from_json(const char *json) {
    json_object *root = NULL, *rootfs = NULL, *mounts = NULL, *proc = NULL;
    config_spec *spec = NULL;
    int i;

    if (!json) return NULL;

    root = json_tokener_parse(json);
    if (!root) return NULL;
    if (!json_object_is_type(root, json_type_object)) goto fail;

    if (!json_object_object_get_ex(root, "mounts", &mounts)) {
        spec = alloc_spec(0);
    } else {
        int mount_count;
        if (!json_object_is_type(mounts, json_type_array)) goto fail;
        mount_count = (int)json_object_array_length(mounts);
        spec = alloc_spec(mount_count);
    }
    if (!spec) goto fail;

    if (get_string_field_json(root, "ociVersion",
            spec->oci_version, sizeof(spec->oci_version), 1) < 0) goto fail;

    if (get_string_field_json(root, "hostname",
            spec->hostname, sizeof(spec->hostname), 0) < 0) goto fail;

    if (!json_object_object_get_ex(root, "root", &rootfs)) goto fail;
    if (!json_object_is_type(rootfs, json_type_object)) goto fail;

    char rootfs_relative[PATH_MAX];
    if (get_string_field_json(rootfs, "path",
            rootfs_relative, sizeof(rootfs_relative), 1) < 0) goto fail;
    if (!realpath(rootfs_relative, spec->rootfs_path)) goto fail;
    if (get_int_field_json(rootfs, "readonly",
            &spec->rootfs_readonly, 0) < 0) goto fail;

    if (mounts) {
        for (i = 0; i < spec->mount_count; ++i) {
            json_object *jm = json_object_array_get_idx(mounts, (size_t)i);
            json_object *jopts = NULL;
            int j, n;

            if (!jm || !json_object_is_type(jm, json_type_object)) goto fail;

            if (get_string_field_json(jm, "destination",
                    spec->mounts[i].destination,
                    sizeof(spec->mounts[i].destination), 1) < 0) goto fail;
            if (get_string_field_json(jm, "source",
                    spec->mounts[i].source,
                    sizeof(spec->mounts[i].source), 1) < 0) goto fail;
            if (get_string_field_json(jm, "type",
                    spec->mounts[i].type,
                    sizeof(spec->mounts[i].type), 1) < 0) goto fail;

            if (json_object_object_get_ex(jm, "options", &jopts)) {
                if (!json_object_is_type(jopts, json_type_array)) goto fail;

                n = (int)json_object_array_length(jopts);
                spec->mounts[i].options_count = n;

                if (n > 0) {
                    spec->mounts[i].options =
                        calloc((size_t)n, sizeof(*spec->mounts[i].options));
                    if (!spec->mounts[i].options) goto fail;

                    for (j = 0; j < n; ++j) {
                        json_object *opt = json_object_array_get_idx(jopts, (size_t)j);
                        const char *s;

                        if (!opt || !json_object_is_type(opt, json_type_string)) goto fail;
                        s = json_object_get_string(opt);
                        spec->mounts[i].options[j] = strdup(s);
                        if (!spec->mounts[i].options[j]) goto fail;
                    }
                }
            }
        }
    }

    if (json_object_object_get_ex(root, "process", &proc)) {
        json_object *jargs = NULL;
        int j, argc;

        if (!json_object_is_type(proc, json_type_object)) goto fail;
        spec->has_process = 1;

        if (get_int_field_json(proc, "terminal", &spec->process.terminal, 0) < 0) goto fail;
        if (get_string_field_json(proc, "cwd",
                spec->process.cwd, sizeof(spec->process.cwd), 1) < 0) goto fail;

        if (json_object_object_get_ex(proc, "args", &jargs)) {
            if (!json_object_is_type(jargs, json_type_array)) goto fail;

            argc = (int)json_object_array_length(jargs);
            spec->process.argc = argc;

            if (argc > 0) {
                spec->process.argv = calloc((size_t)argc, sizeof(*spec->process.argv));
                if (!spec->process.argv) goto fail;

                for (j = 0; j < argc; ++j) {
                    json_object *arg = json_object_array_get_idx(jargs, (size_t)j);
                    const char *s;

                    if (!arg || !json_object_is_type(arg, json_type_string)) goto fail;
                    s = json_object_get_string(arg);
                    spec->process.argv[j] = strdup(s);
                    if (!spec->process.argv[j]) goto fail;
                }
            }
        }
    }

    json_object_put(root);
    return spec;

fail:
    json_object_put(root);
    free_spec(spec);
    return NULL;
}