#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>

#include "log.h"
#include "config.h"
#include "utils.h"

enum verbosity_level _verbosity_level = DEFAULT_VERBOSITY_LEVEL;

int set_verbosity_level(enum verbosity_level level) {
    if (level < 0 || level >= VERBOSITY_COUNT) {
        errno = EINVAL;
        return -1;
    }
    _verbosity_level = level;
    return 0;
}
enum verbosity_level get_verbosity_level(void) {
    return _verbosity_level;
}

/* LOG FORMAT.
    debug  - [DEBUG] <program name>: <message>
    error  - [ERROR] <program name>: <message>
    perror - [ERROR] <program name> <message>: <strerror>
*/

int log_debug(const char *s) {
    if (get_verbosity_level() < VERBOSITY_DEBUG)
        return 0;
    fprintf(stdout, "[DEBUG] %s: %s\n", get_program_name(), s);
    return 1;
}

int log_debugf(const char *fmt, ...) {
    if (get_verbosity_level() < VERBOSITY_DEBUG)
        return 0;
    fprintf(stdout, "[DEBUG] %s: ", get_program_name());

    va_list args;
    va_start(args, fmt);
    vfprintf(stdout, fmt, args);
    va_end(args);

    fprintf(stdout, "\n");
    return 1;
}

int log_error(const char *s) {
    if (get_verbosity_level() < VERBOSITY_ERROR)
        return 0;
    fprintf(stderr, "[ERROR] %s: %s\n", get_program_name(), s);
    return 1;
}

int log_errorf(const char *fmt, ...) {
    if (get_verbosity_level() < VERBOSITY_ERROR)
        return 0;
    fprintf(stderr, "[ERROR] %s: ", get_program_name());

    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);

    fprintf(stderr, "\n");
    return 1;
}

int log_perror(const char *s) {
    if (get_verbosity_level() < VERBOSITY_ERROR)
        return 0;
    fprintf(stderr, "[ERROR] %s %s: %s\n", get_program_name(), s, strerror(errno));
    return 1;
}
