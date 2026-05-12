#include "debuglog.h"
#include "printf.h"

#define DEBUG_LOG_SIZE 16384

static char debug_log[DEBUG_LOG_SIZE];
static size_t debug_log_pos;
static int debug_log_overflow;
static int debug_log_enabled = 0;
static int debug_print_enabled = 0;

void debug_log_init(void) {
    debug_log_pos = 0;
    debug_log_overflow = 0;
    debug_log_enabled = 0;
    debug_print_enabled = 0;
    for (size_t i = 0; i < DEBUG_LOG_SIZE; i++) {
        debug_log[i] = '\0';
    }
}

void debug_log_putchar(char c) {
    if (!debug_log_enabled) {
        return;
    }
    if (debug_log_pos < DEBUG_LOG_SIZE - 1) {
        debug_log[debug_log_pos++] = c;
        debug_log[debug_log_pos] = '\0';
    } else {
        debug_log_overflow = 1;
    }
}

void debug_log_write(const char *str) {
    if (!debug_log_enabled) {
        return;
    }
    if (!str) {
        return;
    }
    for (size_t i = 0; str[i] != '\0'; i++) {
        debug_log_putchar(str[i]);
    }
}

void debug_log_dump(void) {
    printf("\n[DEBUG LOG] %s\n", debug_log);
    if (debug_log_overflow) {
        printf("[DEBUG LOG] buffer overflowed\n");
    }
}

const char *debug_log_buffer(void) {
    return debug_log;
}

size_t debug_log_length(void) {
    return debug_log_pos;
}

void debug_log_set_enabled(int enabled) {
    debug_log_enabled = enabled ? 1 : 0;
}

int debug_log_is_enabled(void) {
    return debug_log_enabled;
}

void debug_print_set_enabled(int enabled) {
    debug_print_enabled = enabled ? 1 : 0;
}

int debug_print_is_enabled(void) {
    return debug_print_enabled;
}
