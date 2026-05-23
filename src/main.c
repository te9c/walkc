#include <stdio.h>
#include <string.h>

#include "container.h"
#include "metadata.h"

#define unused_attr __attribute__((unused))

typedef int (*cmd_fn)(int argc, char **argv);

typedef struct command {
    const char* name;
    cmd_fn run;
    const char* summary;
} command;

static int cmd_create(int argc unused_attr, char** argv unused_attr) { return 1; }
static int cmd_delete(int argc unused_attr, char** argv unused_attr) { return 1; }
static int cmd_run(int argc unused_attr, char** argv unused_attr) { return 1; }
static int cmd_start(int argc unused_attr, char** argv unused_attr) { return 1; }
static int cmd_kill(int argc unused_attr, char** argv unused_attr) { return 1; }
static int cmd_spec(int argc unused_attr, char** argv unused_attr) { return 1; }
static int cmd_help(int argc, char** argv);

static const command commands[] = {
    { "create",    cmd_create,    "create container" },
    { "delete",    cmd_delete,    "delete container" },
    { "run",       cmd_run,       "create and run container" },
    { "start",     cmd_start,     "start previously created container"},
    { "kill",      cmd_kill,      "send signal to container's init process"},
    { "spec",      cmd_spec,      "generate spec file"},
    { "help",      cmd_help,      "show this message"}
};

static const int command_count = sizeof(commands) / sizeof(command);

#define SPACES_PREFIX 2
#define SPACES_MIDDLE 2
static int cmd_help(int argc unused_attr, char** argv unused_attr) {
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

const command* find_command(const char* name) {
    for (int i = 0; i < command_count; ++i) {
        if (strcmp(commands[i].name, name) == 0) {
            return &commands[i];
        }
    }
    return NULL;
}

int main(int argc, char* argv[]) {
    // TODO: global options (-v)
    if (argc < 2) {
        cmd_help(argc, argv);
        return 1;
    }

    const command* cmd = find_command(argv[1]);
    if (!cmd) {
        fprintf(stderr, "Invalid command.\n");
        return 1;
    }
    return cmd->run(argc - 1, argv + 1);
}