#ifndef CONFIG_H
#define CONFIG_H

#include <json-c/json.h>

#define OCI_VERSION "1.0.0"
#define DEFAULT_BUNDLE_PATH "."
#define CONFIG_FILENAME "config.json"
#define STATE_FILENAME "state.json"
#define OCI_VERSION_MAX 32
#define TYPE_MAX 64
#define DEFAULT_ROOTFS_PATH "rootfs"
#define DEFUALT_RUN_PROGRAM "sh"
#define DEFAULT_HOSTNAME "walkc"
#define RUNTIME_DIR_NAME "walkc"
#define FALLBACK_RUNTIME_DIR "/run/" RUNTIME_DIR_NAME
#define ID_MAX 128
#define JSON_TO_STRING_DEFAULT_FLAGS (JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_NOSLASHESCAPE)

// Help is a table with two columns: command and summary.
// Spaces before first column
#define CMD_HELP_SPACES_PREFIX 2
// Spaces between the first and the second columns.
#define CMD_HELP_SPACES_MIDDLE 2

#endif