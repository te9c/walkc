#ifndef CONTAINER_H
#define CONTAINER_H

#define _GNU_SOURCE
#include <sched.h>

#define STACK_SIZE (1024 * 1024)

typedef struct container {
    char *rootfs;
    char **argv;
    char stack[STACK_SIZE];
} container;

int run_container(container *cont);

#endif
