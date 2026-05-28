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

#ifndef DEBUGLOG_H
#define DEBUGLOG_H

#include <stddef.h>

// initialize the debug log buffer
void debug_log_init(void);

// write a single character to the debug log
void debug_log_putchar(char c);

// write a string to the debug log
void debug_log_write(const char *str);

// dump the entire debug log buffer via printf
void debug_log_dump(void);

// get a pointer to the debug log buffer
const char *debug_log_buffer(void);

// get the current length of the debug log
size_t debug_log_length(void);

// enable or disable debug logging
void debug_log_set_enabled(int enabled);

// check if debug logging is enabled
int  debug_log_is_enabled(void);

// control whether kernel print output (printf) is enabled
void debug_print_set_enabled(int enabled);

// check if debug printing is enabled
int  debug_print_is_enabled(void);

#endif
