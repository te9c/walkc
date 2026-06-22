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
#include <libgen.h>

#include "container.h"
#include "spec.h"
#include "config.h"
#include "utils.h"
#include "log.h"

#define unused_arg __attribute__((unused))

#define HANDLE_GETOPT_ERRORS(argv)                                             \
    case '?': {                                                                \
        if (optopt)                                                            \
            log_errorf("Unknown option: -%c", optopt);                         \
        else                                                                   \
            log_errorf("Unknown option: %s", (argv)[optind - 1]);              \
        return 1;                                                              \
    }                                                                          \
    case ':': {                                                                \
        log_errorf("Option -%c requires an argument", optopt);                 \
        return 1;                                                              \
    }

typedef int (*cmd_fn)(int argc, char **argv);

typedef struct command {
    const char *name;
    cmd_fn run;
    const char *summary;
} command;

static int cmd_start_internal(const char *id) {
    const char *rd = runtime_dir();
    if (!rd) {
        log_perror("runtime_dir");
        return 1;
    }
    if (chdir(rd) < 0) {
        log_perror("chdir");
        return 1;
    }
    if (chdir(id) < 0) {
        log_perror("chdir");
        return 1;
    }
    char *state_json = read_all_file(STATE_FILENAME);
    if (!state_json) {
        log_perror("state.json");
        return 1;
    }
    container *cont = container_from_state_json(state_json);
    free(state_json);
    if (!cont) {
        log_perror("parsing of state.json");
        return 1;
    }
    if (cont->status != CONTAINER_CREATED) {
        log_error("Container is not in created status");
        free_spec(cont->spec);
        free(cont);
        return 1;
    }

    // TODO: check container for correctness.
    int ret = run_container(cont);
    free_spec(cont->spec);
    free(cont);
    return ret;
}

static int cmd_create_internal(const char *id, const char *bundle_path) {
    if (strlen(id) >= ID_MAX) {
        log_error("Id is too big");
        return 1;
    }
    if (chdir(bundle_path) < 0) {
        log_perror("chdir");
        return 1;
    }
    container cont;
    cont.status = CONTAINER_CREATED;
    cont.pid = 0;
    strcpy(cont.id, id);

    char *config = read_all_file(CONFIG_FILENAME);
    if (!config) {
        log_perror("config.json");
        return 1;
    }

    if (!realpath(bundle_path, cont.bundle)) {
        log_perror("realpath");
        free(config);
        return 1;
    }
    cont.spec = spec_from_json(config);
    if (!cont.spec) {
        log_perror("spec_from_json");
        free(config);
        return 1;
    }
    free(config);


    const char *rd = runtime_dir();
    if (!rd) {
        log_perror("runtime dir");
        free_spec(cont.spec);
        return 1;
    }

    if (chdir(rd) < 0) {
        log_perror("chdir");
        free_spec(cont.spec);
        return 1;
    }
    if (chdir(id) == 0) {
        log_error("Id already exists");
        free_spec(cont.spec);
        return 1;
    }
    if (mkdir(id, 0700) < 0) {
        log_perror("mkdir");
        free_spec(cont.spec);
        return 1;
    }
    if (chdir(id) < 0) {
        log_perror("chdir");
        free_spec(cont.spec);
        return 1;
    }

    char *container_state_json = container_to_state_json(&cont);
    if (!container_state_json) {
        log_perror("container_to_state_json");
        free_spec(cont.spec);
        return 1;
    }
    if (create_file_with_content(STATE_FILENAME, container_state_json) < 0) {
        free_spec(cont.spec);
        free(container_state_json);
        return 1;
    }

    free_spec(cont.spec);
    free(container_state_json);
    return 0;
}

static int cmd_create(int argc, char **argv) {
    // options:
    // --bundle: path to bundle (default is current directory)
    static struct option long_options[] = {
        {
            .name = "bundle",
            .has_arg = required_argument,
            .flag = NULL,
            .val = 'b'
        },
        {0,0,0,0}
    };
    char bundle_path[PATH_MAX] = DEFAULT_BUNDLE_PATH;
    int c;
    while ((c = getopt_long(argc, argv, ":b:", long_options, NULL)) != -1) {
        switch (c) {
            case 'b': {
                int ln = strlen(optarg);
                if (ln >= PATH_MAX) {
                    log_error("Bundle argument is too big");
                    return 1;
                }
                strcpy(bundle_path, optarg);
                break;
            }
            HANDLE_GETOPT_ERRORS(argv)
        }
    }
    if (optind == argc) {
        log_error("Expected id");
        return 1;
    }
    if (optind + 1 != argc) {
        log_error("Invalid usage");
        return 1;
    }

    char *id = argv[optind];

    return cmd_create_internal(id, bundle_path);
}

static int cmd_delete(int argc, char **argv) {
    if (argc != 2) {
        log_error("Invalid usage");
        return 1;
    }

    char *id = argv[1];
    const char *rd = runtime_dir();
    if (!rd) {
        log_perror("runtime_dir\n");
        return 1;
    }
    if (chdir(rd) < 0) {
        log_perror("chdir\n");
        return 1;
    }
    if (chdir(id) < 0) {
        log_perror("chdir");
        return 1;
    }

    char *state_json = read_all_file(STATE_FILENAME);
    if (!state_json) {
        log_perror("read_all_file");
        return 1;
    }
    container *cont = container_from_state_json(state_json);
    if (!cont) {
        log_perror("container_from_state_json");
        free(state_json);
        return 1;
    }
    if (cont->status != CONTAINER_CREATED && cont->status != CONTAINER_STOPPED) {
        log_error("Container status should be either created or stopped");
        free(state_json);
        free_spec(cont->spec);
        free(cont);
        return 1;
    }
    free(state_json);
    free_spec(cont->spec);
    free(cont);

    if (unlink(STATE_FILENAME) < 0) {
        log_perror("unlink");
        return 1;
    }
    if (chdir(rd) < 0) {
        log_perror("chdir");
        return 1;
    }
    if (rmdir(id) < 0) {
        log_perror("rmdir");
        return 1;
    }

    return 0;
}

static int cmd_run(int argc, char **argv) {
    static struct option long_options[] = {
        {
            .name = "bundle",
            .has_arg = required_argument,
            .flag = NULL,
            .val = 'b'
        },
        {0,0,0,0}
    };
    char bundle_path[PATH_MAX] = DEFAULT_BUNDLE_PATH;
    int c;
    while ((c = getopt_long(argc, argv, "b:", long_options, NULL)) != -1) {
        switch (c) {
            case 'b': {
                int ln = strlen(optarg);
                if (ln >= PATH_MAX) {
                    log_error("Bundle argument is too big");
                    return 1;
                }
                strcpy(bundle_path, optarg);
                break;
            }
            HANDLE_GETOPT_ERRORS(argv)
        }
    }
    if (optind + 1 != argc) {
        log_error("Invalid usage");
        return 1;
    }
    char *id = argv[optind];

    // return 1;

    int ret;
    if ((ret = cmd_create_internal(id, bundle_path)) != 0) {
        return ret;
    }

    if ((ret = cmd_start_internal(id)) != 0) {
        return ret;
    }

    return 0;
}

static int cmd_start(int argc, char **argv) {
    if (argc != 2) {
        log_error("Invalid usage");
        return 1;
    }

    char *id = argv[1];
    return cmd_start_internal(id);
}

static int cmd_kill(int argc, char **argv) {
    if (argc != 2 && argc != 3) {
        log_error("Invalid usage");
        return 1;
    }
    char *id = argv[1];
    char *sig_arg = argc == 3 ? argv[2] : "SIGTERM";

    int signo = sigstr_to_num(sig_arg);
    if (signo < 0) {
        log_error("Invalid signal");
        return 1;
    }

    const char *rd = runtime_dir();
    if (!rd) {
        log_perror("runtime_dir");
        return 1;
    }
    if (chdir(rd) < 0) {
        log_perror("chdir");
        return 1;
    }
    if (chdir(id) < 0) {
        log_perror("chdir");
        return 1;
    }

    char *state_json = read_all_file(STATE_FILENAME);
    if (!state_json) {
        log_perror("state.json");
        return 1;
    }
    container *cont = container_from_state_json(state_json);
    free(state_json);
    if (!cont) {
        log_perror("parsing of state.json");
        return 1;
    }
    if (cont->status != CONTAINER_RUNNING) {
        log_error("Container should be in running state");
        free_spec(cont->spec);
        free(cont);
        return 1;
    }

    pid_t p = cont->pid;
    free_spec(cont->spec);
    free(cont);
    if (kill(p, signo) < 0) {
        log_perror("kill");
        return 1;
    }
    return 0;
}

static int cmd_spec(int argc, char **argv) {
    static struct option long_options[] = {
        {
            .name    = "bundle",            // name of the long option
            .has_arg = required_argument,   // does it require argument?
            .flag    = NULL,                // if null returns val otherwise return 0
            .val     = 'b'                  // what to return if option is found
        },
        {
            .name = "force",
            .has_arg = no_argument,
            .flag = NULL,
            .val = 'f'
        },
        {0,0,0,0}
    };
    char bundle_path[PATH_MAX] = DEFAULT_BUNDLE_PATH;
    int c;
    int forced = 0;
    while ((c = getopt_long(argc, argv, "b:f", long_options, NULL)) != -1) {
        switch (c) {
            case 'b': {
                int ln = strlen(optarg);
                if (ln >= PATH_MAX) {
                    log_error("Bundle argument is too big");
                    return 1;
                }
                strcpy(bundle_path, optarg);
                break;
            }
            case 'f': {
                forced = 1;
                break;
            }
            HANDLE_GETOPT_ERRORS(argv)
        }
    }
    if (optind < argc) {
        log_error("Invalid usage");
        return 1;
    }
    if (chdir(bundle_path) < 0) {
        log_perror("chdir");
        return 1;
    }
    if (!forced && access(CONFIG_FILENAME, F_OK) == 0) {
        log_error("config.json already exists");
        return 1;
    }
    config_spec *spec = get_default_spec();
    if (!spec) {
        log_perror("default_spec");
        return 1;
    }
    char *json_spec = spec_to_json(spec, JSON_TO_STRING_DEFAULT_FLAGS);
    free_spec(spec);
    if (create_file_with_content(CONFIG_FILENAME, json_spec) < 0) {
        log_perror("config.json");
        return 1;
    }
    free(json_spec);
    return 0;
}

static int cmd_state(int argc, char **argv) {
    if (argc != 2) {
        log_error("invalid usage");
        return 1;
    }
    char *id = argv[1];
    const char *rd = runtime_dir();
    if (!rd) {
        log_perror("runtime_dir");
        return 1;
    }
    if (chdir(rd) < 0) {
        log_perror("chdir");
        return 1;
    }
    if (chdir(id) < 0) {
        log_perror("chdir");
        return 1;
    }
    char *state_json = read_all_file(STATE_FILENAME);
    printf("%s\n", state_json);
    free(state_json);
    
    return 0;
}

static int cmd_help(int argc, char **argv);

static const command commands[] = {
  /*{ <name>,      <run_cmd>,     <summary>           }*/
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

static int cmd_help(int argc unused_arg, char **argv unused_arg) {
    int longest_command = 0;
    for (int i = 0; i < command_count; ++i) {
        if ((int)strlen(commands[i].name) > longest_command) {
            longest_command = strlen(commands[i].name);
        }
    }
    
    printf("%s - educational container runtime.\n", get_program_name());
    printf("COMMANDS:\n");
    for (int i = 0; i < command_count; ++i) {
        for (int j = 0; j < CMD_HELP_SPACES_PREFIX; ++j)
            putchar(' ');
        printf("%s", commands[i].name);
        int len = strlen(commands[i].name);
        int spaces_needed = longest_command - len + CMD_HELP_SPACES_MIDDLE;
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
    if (argc == 0) return 1;

    char *name = basename(argv[0]);
    if (set_program_name(name) < 0) {
        if (set_program_name(FALLBACK_PROGRAM_NAME) < 0) {
            log_perror("set_program_name");
            return 1;
        }
    }

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
        {
            .name = "verbose",
            .has_arg = no_argument,
            .flag = NULL,
            .val = 'v'
        },
        {
            .name = "quiet",
            .has_arg = no_argument,
            .flag = NULL,
            .val = 'q'
        },
        {0,0,0,0}
    };

    opterr = 0;
    while ((c = getopt_long(global_argc, argv, "vq", long_options, NULL)) != -1) {
        switch (c) {
            case 'v': {
                set_verbosity_level(VERBOSITY_DEBUG);
                break;
            }
            case 'q': {
                set_verbosity_level(VERBOSITY_QUIET);
                break;
            }
            HANDLE_GETOPT_ERRORS(argv)
        }
    }
    if (global_argc == argc) {
        log_error("Invalid usage");
        return 1;
    }

    const command *cmd = find_command(argv[global_argc]);
    if (!cmd) {
        log_error("Invalid command");
        return 1;
    }
    optind = 1;
    return cmd->run(argc - global_argc, argv + global_argc);
}
