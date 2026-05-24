#ifndef CONTAINER_H
#define CONTAINER_H

#define _GNU_SOURCE
#include "spec.h"
#include "config.h"

#include <sched.h>

typedef enum container_status {
    CONTAINER_CREATING = 0,
    CONTAINER_CREATED,
    CONTAINER_RUNNING,
    CONTAINER_STOPPED
} container_status;


typedef struct container {
    container_status status;
    pid_t pid;

    char id[ID_MAX];
    char bundle[PATH_MAX];
    config_spec *spec;
} container;

int run_container(container *cont);

const char *container_status_to_string(container_status st);
char *container_to_state_json(container *cont);
container container_from_state_json(const char* state);
#endif