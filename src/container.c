#define _GNU_SOURCE
#include "container.h"

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

static int sys_pivot_root(const char *new_root, const char *put_old) {
    return syscall(SYS_pivot_root, new_root, put_old);
}

static int mkdir_if_needed(const char *path, mode_t mode) {
    if (mkdir(path, mode) == -1 && errno != EEXIST) {
        return -1;
    }
    return 0;
}

static int container_start(void *arg) {
    container *cont = (container *)arg;
    char old_root[PATH_MAX];

    if (!cont || !cont->rootfs || !cont->argv || !cont->argv[0]) {
        fprintf(stderr, "invalid container config\n");
        return 1;
    }

    if (mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL) == -1) {
        perror("mount MS_PRIVATE");
        return 1;
    }

    if (mount(cont->rootfs, cont->rootfs, NULL, MS_BIND | MS_REC, NULL) == -1) {
        perror("bind-mount rootfs");
        return 1;
    }

    if (snprintf(old_root, sizeof(old_root), "%s/.old_root", cont->rootfs) >= (int)sizeof(old_root)) {
        fprintf(stderr, "rootfs path too long\n");
        return 1;
    }

    if (mkdir_if_needed(old_root, 0755) == -1) {
        perror("mkdir .old_root");
        return 1;
    }

    if (sys_pivot_root(cont->rootfs, old_root) == -1) {
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

    if (mkdir_if_needed("/proc", 0555) == -1) {
        perror("mkdir /proc");
        return 1;
    }

    if (mount("proc", "/proc", "proc", 0, NULL) == -1) {
        perror("mount /proc");
        return 1;
    }

    char* command = cont->argv[0];
    cont->argv[0] = basename(cont->argv[0]);
    execvp(command, cont->argv);
    perror("execvp");
    return 1;
}

int run_container(container *cont) {
    pid_t pid;
    int status;

    if (!cont) {
        fprintf(stderr, "container is NULL\n");
        return 1;
    }

    pid = clone(
        container_start,
        cont->stack + STACK_SIZE,
        CLONE_NEWNS | CLONE_NEWPID | CLONE_NEWUTS | CLONE_NEWIPC | SIGCHLD,
        cont
    );

    if (pid == -1) {
        perror("clone");
        return 1;
    }

    if (waitpid(pid, &status, 0) == -1) {
        perror("waitpid");
        return 1;
    }

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }

    if (WIFSIGNALED(status)) {
        fprintf(stderr, "container killed by signal %d\n", WTERMSIG(status));
    }

    return 1;
}
