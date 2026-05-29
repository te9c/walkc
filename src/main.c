#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <signal.h>

#include "container.h"
#include "spec.h"
#include "config.h"
#include "utils.h"

#define unused_attr __attribute__((unused))

typedef int (*cmd_fn)(int argc, char **argv);

typedef struct command {
    const char *name;
    cmd_fn run;
    const char *summary;
} command;

static int cmd_start_internal(const char *id) {
    const char *rd = runtime_dir();
    if (!rd) {
        perror("runtime_dir");
        return 1;
    }
    if (chdir(rd) < 0) {
        perror("chdir");
        return 1;
    }
    if (chdir(id) < 0) {
        perror("chdir");
        return 1;
    }
    char *state_json = read_all_file(STATE_FILENAME);
    if (!state_json) {
        perror("state.json");
        return 1;
    }
    container *cont = container_from_state_json(state_json);
    if (!cont) {
        perror("parsing of state.json");
        return 1;
    }
    if (cont->status != CONTAINER_CREATED) {
        fprintf(stderr, "Container is not in created status.\n");
        return 1;
    }

    // TODO: check container for correctness.
    return run_container(cont);
}

static int cmd_create_internal(const char *id, const char *bundle_path) {
    if (strlen(id) >= ID_MAX) {
        fprintf(stderr, "Id is too big.\n");
        return 1;
    }
    if (chdir(bundle_path) < 0) {
        perror("chdir");
        return 1;
    }
    char *spec_file_contents = read_all_file(CONFIG_FILENAME);
    if (!spec_file_contents) {
        perror("config.json");
        return 1;
    }

    container cont;
    cont.status = CONTAINER_CREATED;
    cont.pid = 0;
    strcpy(cont.id, id);
    if (!realpath(bundle_path, cont.bundle)) {
        perror("realpath");
        return 1;
    }
    cont.spec = spec_from_json(spec_file_contents);
    if (!cont.spec) {
        perror("spec_from_json");
        return 1;
    }


    const char *rd = runtime_dir();
    if (!rd) {
        perror("runtime dir");
        return 1;
    }

    if (chdir(rd) < 0) {
        perror("chdir");
        return 1;
    }
    if (chdir(id) == 0) {
        fprintf(stderr, "id already exists\n");
        return 1;
    }
    if (mkdir(id, 0700) < 0) {
        perror("mkdir");
        return 1;
    }
    if (chdir(id) < 0) {
        perror("chdir");
        return 1;
    }

    char *container_state_json = container_to_state_json(&cont);
    if (!container_state_json) {
        perror("container_to_state_json");
        return 1;
    }
    if (create_file_with_content(STATE_FILENAME, container_state_json) < 0) {
        return 1;
    }
    return 0;
}

static int cmd_create(int argc, char **argv) {
    // options:
    // --bundle: path to bundle (default is current directory)
    static struct option long_options[] = {
        {"bundle", required_argument, 0, 'b'},
        {0,0,0,0}
    };
    char bundle_path[PATH_MAX] = DEFAULT_BUNDLE_PATH;
    int c;
    while ((c = getopt_long(argc, argv, "b:", long_options, NULL)) != -1) {
        switch (c) {
            case 'b': {
                int ln = strlen(optarg);
                if (ln >= PATH_MAX) {
                    fprintf(stderr, "bundle argument is too big.\n");
                    return 1;
                }
                strcpy(bundle_path, optarg);
                break;
            }
            case '?':
                return 1;
        }
    }
    if (optind == argc) {
        fprintf(stderr, "Expected id.\n");
        return 1;
    }
    if (optind + 1 != argc) {
        fprintf(stderr, "Invalid usage.\n");
    }

    char *id = argv[optind];

    return cmd_create_internal(id, bundle_path);
}
static int cmd_delete(int argc unused_attr, char **argv unused_attr) {
    if (argc != 2) {
        fprintf(stderr, "Invalid usage.\n");
        return 1;
    }

    char *id = argv[1];
    const char *rd = runtime_dir();
    if (!rd) {
        perror("runtime_dir\n");
        return 1;
    }
    if (chdir(rd) < 0) {
        perror("chdir\n");
    }
    if (chdir(id) < 0) {
        perror("chdir");
    }

    char *state_json = read_all_file(STATE_FILENAME);
    if (!state_json) {
        perror("read_all_file");
        return 1;
    }
    container *cont = container_from_state_json(state_json);
    if (!cont) {
        perror("container_from_state_json");
        return 1;
    }
    if (cont->status != CONTAINER_CREATED && cont->status != CONTAINER_STOPPED) {
        fprintf(stderr, "Container status should be either created or stopped\n");
        return 1;
    }

    if (unlink(STATE_FILENAME) < 0) {
        perror("unlink");
        return 1;
    }
    if (chdir(rd) < 0) {
        perror("chdir");
        return 1;
    }
    if (rmdir(id) < 0) {
        perror("rmdir");
        return 1;
    }
    return 0;
}
static int cmd_run(int argc unused_attr, char **argv unused_attr) {
    static struct option long_options[] = {
        {"bundle", required_argument, 0, 'b'},
        {0,0,0,0}
    };
    char bundle_path[PATH_MAX] = DEFAULT_BUNDLE_PATH;
    int c;
    while ((c = getopt_long(argc, argv, "b:", long_options, NULL)) != -1) {
        switch (c) {
            case 'b': {
                int ln = strlen(optarg);
                if (ln >= PATH_MAX) {
                    fprintf(stderr, "bundle argument is too big.\n");
                    return 1;
                }
                strcpy(bundle_path, optarg);
                break;
            }
            case '?':
                return 1;
        }
    }
    if (optind + 1 != argc) {
        fprintf(stderr, "Invalid usage.\n");
        return 1;
    }
    char *id = argv[optind];

    int ret;
    if ((ret = cmd_create_internal(id, bundle_path)) != 0) {
        return ret;
    }
    if ((ret = cmd_start_internal(id)) != 0) {
        return ret;
    }

    return 0;
}

static int cmd_start(int argc unused_attr, char **argv unused_attr) {
    if (argc != 2) {
        fprintf(stderr, "Invalid usage.\n");
        return 1;
    }

    char *id = argv[1];
    return cmd_start_internal(id);
}
static int cmd_kill(int argc unused_attr, char **argv unused_attr) {
    if (argc != 2 && argc != 3) {
        fprintf(stderr, "Invalid usage\n");
        return 1;
    }
    char *id = argv[1];
    char *sig_arg = argc == 3 ? argv[2] : "SIGTERM";

    int signo = sigstr_to_num(sig_arg);
    if (signo < 0) {
        fprintf(stderr, "Invalid signal\n");
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
    if (chdir(id) < 0) {
        perror("chdir");
        return 1;
    }

    char *state_json = read_all_file(STATE_FILENAME);
    if (!state_json) {
        perror("state.json");
        return 1;
    }
    container *cont = container_from_state_json(state_json);
    if (!cont) {
        perror("parsing of state.json");
        return 1;
    }
    if (cont->status != CONTAINER_RUNNING) {
        fprintf(stderr, "container should be in running state");
        return 1;
    }

    pid_t p = cont->pid;
    if (kill(p, signo) < 0) {
        perror("kill");
        return 1;
    }
    return 0;
}
static int cmd_spec(int argc, char **argv) {
    static struct option long_options[] = {
        {"bundle", required_argument, 0, 'b'},
        {0,0,0,0}
    };
    char bundle_path[PATH_MAX] = DEFAULT_BUNDLE_PATH;
    int c;
    while ((c = getopt_long(argc, argv, "b:", long_options, NULL)) != -1) {
        switch (c) {
            case 'b': {
                int ln = strlen(optarg);
                if (ln >= PATH_MAX) {
                    fprintf(stderr, "bundle argument is too big.\n");
                    return 1;
                }
                strcpy(bundle_path, optarg);
                break;
            }
            case '?':
                return 1;
        }
    }
    if (optind < argc) {
        fprintf(stderr, "Invalid usage.\n");
        return 1;
    }
    if (chdir(bundle_path) < 0) {
        perror("chdir");
        return 1;
    }
    if (access(CONFIG_FILENAME, F_OK) == 0) {
        fprintf(stderr, "config.json already exists\n");
        return 1;
    }
    config_spec* spec = get_default_spec();
    if (!spec) {
        perror("default_spec");
        return 1;
    }
    char *json_spec = spec_to_json(spec);
    if (create_file_with_content(CONFIG_FILENAME, json_spec) < 0) {
        perror("config.json");
        return 1;
    }
    return 0;
}
static int cmd_state(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "invalid usage\n");
        return 1;
    }
    char *id = argv[1];
    const char *rd = runtime_dir();
    if (!rd) {
        perror("runtime_dir");
        return 1;
    }
    if (chdir(rd) < 0) {
        perror("chdir");
        return 1;
    }
    if (chdir(id) < 0) {
        perror("chdir");
        return 1;
    }
    char *state_json = read_all_file(STATE_FILENAME);
    printf("%s\n", state_json);
    
    return 0;
}
static int cmd_help(int argc, char **argv);

static const command commands[] = {
    { "create",    cmd_create,    "create container" },
    { "delete",    cmd_delete,    "delete container" },
    { "run",       cmd_run,       "create and run container" },
    { "start",     cmd_start,     "start previously created container"},
    { "kill",      cmd_kill,      "send signal to container's init process"},
    { "spec",      cmd_spec,      "generate spec file"},
    { "state",     cmd_state,     "inspect state of the container"},
    { "help",      cmd_help,      "show this message"}
};

static const int command_count = sizeof(commands) / sizeof(command);

#define SPACES_PREFIX 2
#define SPACES_MIDDLE 2
static int cmd_help(int argc unused_attr, char **argv unused_attr) {
    int longest_command = 0;
    for (int i = 0; i < command_count; ++i) {
        if ((int)strlen(commands[i].name) > longest_command) {
            longest_command = strlen(commands[i].name);
        }
    }
    
    printf("walkc - educational container runtime.\n");
    printf("COMMANDS:\n");
    for (int i = 0; i < command_count; ++i) {
        for (int j = 0; j < SPACES_PREFIX; ++j)
            putchar(' ');
        printf("%s", commands[i].name);
        int len = strlen(commands[i].name);
        int spaces_needed = longest_command - len + SPACES_MIDDLE;
        for (int j = 0; j < spaces_needed; ++j) {
            putchar(' ');
        }
        printf("%s\n", commands[i].summary);
    }
    return 0;
}

const command *find_command(const char *name) {
    for (int i = 0; i < command_count; ++i) {
        if (strcmp(commands[i].name, name) == 0) {
            return &commands[i];
        }
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    // TODO: global options (-v)
    if (argc < 2) {
        cmd_help(argc, argv);
        return 1;
    }

    int global_argc = 1;
    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] == '-')
            ++global_argc;
        else
            break;
    }

    int c;
    static struct option long_options[] = {
        {0,0,0,0}
    };
    while ((c = getopt_long(global_argc, argv, "", long_options, NULL)) != -1) {
        switch (c) {
            case '?':
                return 1;
        }
    }
    if (global_argc == argc) {
        fprintf(stderr, "Invalid usage.\n");
        return 1;
    }

    const command *cmd = find_command(argv[1]);
    if (!cmd) {
        fprintf(stderr, "Invalid command.\n");
        return 1;
    }
    optind = 1;
    return cmd->run(argc - global_argc, argv + global_argc);
}