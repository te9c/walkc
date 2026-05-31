#include "spec.h"
#include "utils.h"

#include <string.h>
#include <stdlib.h>
#include <json-c/json.h>
#include <sys/mount.h>


config_spec *alloc_spec(int mount_count) {
    if (mount_count < 0)
        return NULL;
    config_spec *spec = malloc(sizeof(config_spec));

    if (!spec)
        return NULL;
    
    memset(spec, 0, sizeof(config_spec));
    spec->mount_count = mount_count;
    spec->mounts = malloc(sizeof(mount_entry) * mount_count);
    if (!spec->mounts) {
        free(spec);
        return NULL;
    }
    memset(spec->mounts, 0, mount_count * sizeof(mount_entry));

    return spec;
}

void free_spec(config_spec *spec) {
    if (!spec)
        return;
    
    if (spec->mounts) {
        for (int i = 0; i < spec->mount_count; ++i) {
            if (spec->mounts[i].options) {
                for (int j = 0; j < spec->mounts[i].option_count; ++j) {
                    free(spec->mounts[i].options[j]);
                }
                free(spec->mounts[i].options);
            }
        }
        free(spec->mounts);
    }

    if (spec->process.arguments) {
        for (int i = 0; i < spec->process.argument_count; ++i) {
            free(spec->process.arguments[i]);
        }
        free(spec->process.arguments);
    }
    if (spec->process.env) {
        for (int i = 0; i < spec->process.env_count; ++i) {
            free(spec->process.env[i]);
        }
        free(spec->process.env);
    }
    free(spec);
}

int fill_mount(mount_entry *opt, const char *dest,
            const char *type, const char *source, const char *options[], int option_count) {
    if (!opt) return 1;
    memset(opt, 0, sizeof(*opt));
    if (dest) { strcpy(opt->destination, dest); }
    if (type) { strcpy(opt->type, type); }
    if (source) { strcpy(opt->source, source); }
    if (options) {
        opt->option_count = option_count;
        opt->options = calloc(option_count, sizeof(char *));
        if (!opt->options) return 1;
        for (int i = 0; i < option_count; ++i) {
            opt->options[i] = strdup(options[i]);
            if (!opt->options[i]) return 1;
        }
    }
    return 0;
}

config_spec *get_default_spec(void) {
    config_spec *spec = alloc_spec(5);
    if (!spec)
        return NULL;

    strcpy(spec->oci_version, OCI_VERSION);
    strcpy(spec->rootfs_path, DEFAULT_ROOTFS_PATH);
    spec->rootfs_readonly = 0;

    if (fill_mount(
            &spec->mounts[0],
            "/proc",
            "proc",
            "proc",
            NULL,
            0
    ) < 0) {
        free_spec(spec);
        return NULL;
    }

    if (fill_mount(
            &spec->mounts[1],
            "/dev",
            "devtmpfs",
            "dev",
            (const char *[]) { "nosuid", "strictatime", "mode=755", "size=65536k" },
            4) < 0) {
        free_spec(spec);
        return NULL;
    }

    if (fill_mount(
            &spec->mounts[2],
            "/dev/pts",
            "devpts",
            "devpts",
            (const char *[]) { "nosuid", "noexec", "newinstance", "ptmxmode=0666", "mode=0620", "gid=5" },
            6) < 0) {
        free_spec(spec);
        return NULL;
    }

    if (fill_mount(
            &spec->mounts[3],
            "/dev/shm",
            "tmpfs",
            "shm",
            (const char *[]) { "nosuid", "noexec", "nodev", "mode=1777", "size=65536k" },
            5) < 0) {
        free_spec(spec);
        return NULL;
    }

    if (fill_mount(
        &spec->mounts[4],
        "/sys",
        "sysfs",
        "sysfs",
        (const char *[]) { "nosuid", "noexec", "nodev", "ro" },
        4) < 0) {
        free_spec(spec);
        return NULL;
    }


    strcpy(spec->process.cwd, "/");
    spec->process.arguments = malloc(sizeof(char *));
    if (!spec->process.arguments) {
        free_spec(spec);
        return NULL;
    }
    spec->process.arguments[0] = malloc(sizeof(DEFUALT_RUN_PROGRAM));
    if (!spec->process.arguments[0]) {
        free_spec(spec);
        return NULL;
    }
    strcpy(spec->process.arguments[0], DEFUALT_RUN_PROGRAM);
    spec->process.argument_count = 1;

    spec->process.env = calloc(2, sizeof(*spec->process.env));
    if (!spec->process.env) {
        free_spec(spec);
        return NULL;
    }
    spec->process.env[0] = strdup("PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin");
    spec->process.env[1] = strdup("TERM=xterm");
    spec->process.env_count = 2;
    if (!spec->process.env[0] || !spec->process.env[1]) {
        free_spec(spec);
        return NULL;
    }

    spec->process.terminal = 1;
    spec->has_process = 1;

    strcpy(spec->hostname, DEFAULT_HOSTNAME);
    return spec;
}

char *spec_to_json(const config_spec *spec, int flags) {
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

        json_object_object_add(jm, "destination",
            json_object_new_string(spec->mounts[i].destination));
        json_object_object_add(jm, "source",
            json_object_new_string(spec->mounts[i].source));
        json_object_object_add(jm, "type",
            json_object_new_string(spec->mounts[i].type));

        if (spec->mounts[i].options) {
            json_object *jopts = json_object_new_array();
            for (j = 0; j < spec->mounts[i].option_count; ++j) {
                json_object_array_add(jopts,
                    json_object_new_string(spec->mounts[i].options[j]));
            }
            json_object_object_add(jm, "options", jopts);
        }

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

        for (i = 0; i < spec->process.argument_count; ++i) {
            json_object_array_add(jargv,
                json_object_new_string(spec->process.arguments[i]));
        }

        if (spec->process.env_count) {
            json_object *jenv = json_object_new_array();
            for (i = 0; i < spec->process.env_count; ++i) {
                json_object_array_add(jenv,
                    json_object_new_string(spec->process.env[i]));
            }
            json_object_object_add(proc, "env", jenv);
        }

        json_object_object_add(proc, "args", jargv);
        json_object_object_add(root, "process", proc);
    }

    tmp = json_object_to_json_string_ext(root, flags);
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
                spec->mounts[i].option_count = n;

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
        json_object *jargs = NULL, *jenv = NULL;
        int j, argc, envc;

        if (!json_object_is_type(proc, json_type_object)) goto fail;
        spec->has_process = 1;

        if (get_int_field_json(proc, "terminal", &spec->process.terminal, 0) < 0) goto fail;
        if (get_string_field_json(proc, "cwd",
                spec->process.cwd, sizeof(spec->process.cwd), 1) < 0) goto fail;

        if (json_object_object_get_ex(proc, "args", &jargs)) {
            if (!json_object_is_type(jargs, json_type_array)) goto fail;

            argc = (int)json_object_array_length(jargs);
            spec->process.argument_count = argc;

            if (argc > 0) {
                spec->process.arguments = calloc((size_t)argc + 1, sizeof(*spec->process.arguments));
                if (!spec->process.arguments) goto fail;
                spec->process.arguments[argc] = NULL;

                for (j = 0; j < argc; ++j) {
                    json_object *arg = json_object_array_get_idx(jargs, (size_t)j);
                    const char *s;

                    if (!arg || !json_object_is_type(arg, json_type_string)) goto fail;
                    s = json_object_get_string(arg);
                    spec->process.arguments[j] = strdup(s);
                    if (!spec->process.arguments[j]) goto fail;
                }
            }
        }
        if (json_object_object_get_ex(proc, "env", &jenv)) {
            if (!json_object_is_type(jenv, json_type_array)) goto fail;

            envc = (int)json_object_array_length(jenv);
            spec->process.env_count = envc;

            if (envc > 0) {
                spec->process.env = calloc((size_t)envc + 1, sizeof(*spec->process.env));
                if (!spec->process.env) goto fail;
                spec->process.env[envc] = NULL;

                for (j = 0; j < envc; ++j) {
                    json_object *env = json_object_array_get_idx(jenv, (size_t)j);
                    const char *s;

                    if (!env || !json_object_is_type(env, json_type_string)) goto fail;
                    s = json_object_get_string(env);
                    spec->process.env[j] = strdup(s);
                    if (!spec->process.env[j]) goto fail;
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

int apply_mount_option(int *flags, const char *option) {
    if (!option) return 0;
    int dummy = 0;
    if (!flags)
        flags = &dummy;

    if (strcmp(option, "ro") == 0) {
        *flags |= MS_RDONLY;
    } else if (strcmp(option, "rw") == 0) {
        *flags &= ~MS_RDONLY;
    } else if (strcmp(option, "nosuid") == 0) {
        *flags |= MS_NOSUID;
    } else if (strcmp(option, "suid") == 0) {
        *flags &= ~MS_NOSUID;
    } else if (strcmp(option, "noexec") == 0) {
        *flags |= MS_NOEXEC;
    } else if (strcmp(option, "exec") == 0) {
        *flags &= ~MS_NOEXEC;
    } else if (strcmp(option, "nodev") == 0) {
        *flags |= MS_NODEV;
    } else if (strcmp(option, "dev") == 0) {
        *flags &= ~MS_NODEV;
    } else if (strcmp(option, "noatime") == 0) {
        *flags |= MS_NOATIME;
        *flags &= ~(MS_RELATIME | MS_STRICTATIME);
    } else if (strcmp(option, "nodiratime") == 0) {
        *flags |= MS_NODIRATIME;
    } else if (strcmp(option, "relatime") == 0) {
        *flags |= MS_RELATIME;
        *flags &= ~(MS_NOATIME | MS_STRICTATIME);
    } else if (strcmp(option, "strictatime") == 0) {
        *flags |= MS_STRICTATIME;
        *flags &= ~(MS_NOATIME | MS_RELATIME);
    } else {
        return 0;
    }

    return 1;
}
