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

#include "printf.h"
#include "debuglog.h"
#include "../drivers/display/vga.h"
#include "../drivers/serial/serial.h"
#include <stdarg.h>

// write a single character to vga, debug log, and serial
static void put_char(char c) {
    vga_putchar_raw(c);
    debug_log_putchar(c);
    // push to serial ring buffer — returns instantly, no port i/o wait
    if (debug_log_is_enabled()) {
        serial_putchar(c);
    }
}

// print a number in a given base, with optional padding
static void print_int(long val, int base, int width, char pad) {
    char buf[64];
    int i = 0;
    int is_neg = 0;

    if (val == 0) {
        buf[i++] = '0';
    } else {
        if (base == 10 && val < 0) { is_neg = 1; val = -val; }

        while (val != 0) {
            int rem = val % base;
            buf[i++] = (rem > 9) ? (rem - 10) + 'a' : rem + '0';
            val /= base;
        }
    }
    
    if (is_neg) buf[i++] = '-';
    
    // figure out how many padding characters we need
    int num_len = i;
    int pad_count = (width > num_len) ? (width - num_len) : 0;
    
    // left-pad with the padding character
    for (int j = 0; j < pad_count; j++) {
        put_char(pad);
    }
    
    // then print the number (reversed from buffer)
    while (i > 0) put_char(buf[--i]);
}

// main printf function - handles %s, %d, %x, %p, %c, and %%
void printf(const char *format, ...) {
    va_list args;
    va_start(args, format);

    while (*format) {
        if (*format == '%') {
            format++;
            
            // parse optional width and padding
            int width = 0;
            char pad = ' ';
            if (*format == '0') {
                pad = '0';
                format++;
            }
            while (*format >= '0' && *format <= '9') {
                width = width * 10 + (*format - '0');
                format++;
            }
            
            switch (*format) {
                case 's': {
                    const char *s = va_arg(args, const char*);
                    for (int i = 0; s[i]; i++) put_char(s[i]);
                    break;
                }
                case 'd': print_int((long)va_arg(args, int), 10, width, pad); break;
                case 'x': print_int((long)(unsigned)va_arg(args, unsigned int), 16, width, pad); break;
                case 'p':
                    put_char('0'); put_char('x');
                    print_int((long)va_arg(args, void*), 16, width, pad);
                    break;
                case 'c': put_char((char)va_arg(args, int)); break;
                case '%': put_char('%'); break;
                default:  put_char('%'); put_char(*format); break;
            }
        } else {
            put_char(*format);
        }
        format++;
    }

    va_end(args);
    // cursor update is deferred — call vga_flush() explicitly when needed
}
