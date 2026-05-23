#include <stdio.h>

#include "container.h"

void print_help() {
    printf("walkc - educational container runtime\n");
    printf("Available options:\n");
    printf("run - run container\n");
    printf("create - create container\n");
    printf("spec - create configuration file\n");
}

int main(int argc, char* argv[]) {
    // print_help();
    if (argc != 3) {
        printf("invalid usage.\n");
        return 1;
    }

    char* rootfs = argv[1];
    char* command = argv[2];
    char* command_argv[] = {command, NULL};

    container cont;
    cont.rootfs = rootfs;
    cont.argv = command_argv;

    run_container(&cont);

    return 0;
}