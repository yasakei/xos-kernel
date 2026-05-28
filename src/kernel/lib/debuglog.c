// -------------------------------------------------------------------
// mit license
// 
// copyright (c) 2026 xos
// 
// permission is hereby granted, free of charge, to any person
// obtaining a copy of this software and associated documentation
// files (the "software"), to deal in the software without
// restriction, including without limitation the rights to use,
// copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the software, and to permit persons to whom the
// software is furnished to do so, subject to the following
// conditions:
// 
// the above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the software.
// 
// the software is provided "as is", without warranty of any kind,
// express or implied, including but not limited to the warranties
// of merchantability, fitness for a particular purpose and
// noninfringement. in no event shall the authors or copyright
// holders be liable for any claim, damages or other liability,
// whether in an action of contract, tort or otherwise, arising
// from, out of or in connection with the software or the use or
// other dealings in the software.
// -------------------------------------------------------------------

#include "debuglog.h"
#include "printf.h"

#define DEBUG_LOG_SIZE 16384

#ifndef DEBUG_LOG_DEFAULT
#define DEBUG_LOG_DEFAULT 1
#endif

#ifndef DEBUG_PRINT_DEFAULT
#define DEBUG_PRINT_DEFAULT 0
#endif

static char debug_log[DEBUG_LOG_SIZE];
static size_t debug_log_pos;
static int debug_log_overflow;
static int debug_log_enabled = 0;
static int debug_print_enabled = 0;

// initialize the debug log buffer - clears everything and sets defaults
void debug_log_init(void) {
    debug_log_pos = 0;
    debug_log_overflow = 0;
    debug_log_enabled = DEBUG_LOG_DEFAULT ? 1 : 0;
    debug_print_enabled = DEBUG_PRINT_DEFAULT ? 1 : 0;
    for (size_t i = 0; i < DEBUG_LOG_SIZE; i++) {
        debug_log[i] = '\0';
    }
}

// add a single character to the debug log buffer
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

// write a string to the debug log buffer
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

// dump the entire debug log via printf
void debug_log_dump(void) {
    printf("\n[DEBUG LOG] %s\n", debug_log);
    if (debug_log_overflow) {
        printf("[DEBUG LOG] buffer overflowed\n");
    }
}

// get a pointer to the debug log buffer
const char *debug_log_buffer(void) {
    return debug_log;
}

// get the current length of the log
size_t debug_log_length(void) {
    return debug_log_pos;
}

// enable or disable debug logging
void debug_log_set_enabled(int enabled) {
    debug_log_enabled = enabled ? 1 : 0;
}

// check if debug logging is on
int debug_log_is_enabled(void) {
    return debug_log_enabled;
}

// enable or disable debug print output
void debug_print_set_enabled(int enabled) {
    debug_print_enabled = enabled ? 1 : 0;
}

// check if debug printing is on
int debug_print_is_enabled(void) {
    return debug_print_enabled;
}
