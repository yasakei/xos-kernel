#ifndef DEBUGLOG_H
#define DEBUGLOG_H

#include <stddef.h>

void debug_log_init(void);
void debug_log_putchar(char c);
void debug_log_write(const char *str);
void debug_log_dump(void);
const char *debug_log_buffer(void);
size_t debug_log_length(void);

#endif
