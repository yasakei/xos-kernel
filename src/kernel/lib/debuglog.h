#ifndef DEBUGLOG_H
#define DEBUGLOG_H

#include <stddef.h>

void debug_log_init(void);
void debug_log_putchar(char c);
void debug_log_write(const char *str);
void debug_log_dump(void);
const char *debug_log_buffer(void);
size_t debug_log_length(void);
void debug_log_set_enabled(int enabled);
int  debug_log_is_enabled(void);

/* Control whether kernel print output (printf) is enabled. */
void debug_print_set_enabled(int enabled);
int  debug_print_is_enabled(void);

#endif
