#ifndef LOG_H
#define LOG_H

enum verbosity_level {
    VERBOSITY_QUIET = 0,  // no errors printed
    VERBOSITY_ERROR,      // Normal mode - only errors are printed.
    VERBOSITY_DEBUG,      // Print debug informaiton
    VERBOSITY_COUNT
};

int set_verbosity_level(enum verbosity_level level);
enum verbosity_level get_verbosity_level(void);

// These functions return 1 if message was written and 0 otherwise

int log_debug(const char *s);
int log_debugf(const char *fmt, ...);
int log_error(const char *s);
int log_errorf(const char *fmt, ...);
int log_perror(const char *s);

#endif