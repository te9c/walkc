#ifndef CONFIG_H
#define CONFIG_H

#define OCI_VERSION "1.0.0"
#define DEFAULT_BUNDLE_PATH "."
#define SPEC_CONFIG_FILENAME "config.json"
#define STATE_FILENAME "state.json"
#define OCI_VERSION_MAX 32
#define TYPE_MAX 64
#define DEFAULT_ROOTFS_PATH "rootfs"
#define DEFUALT_RUN_PROGRAM "sh"
#define DEFAULT_HOSTNAME "walkc"
#define FALLBACK_RUNTIME_DIR "/run/walkc"
#define RUNTIME_DIR_NAME "walkc"
#define ID_MAX 128
#define CONTAINER_STACK_SIZE (1024 * 1024)

#endif