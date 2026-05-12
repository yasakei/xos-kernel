#include "printf.h"
#include "debuglog.h"
#include "../drivers/display/vga.h"
#include "../drivers/serial/serial.h"
#include <stdarg.h>

// Write char to VGA (raw, no cursor) + debug log + serial TX queue (no wait)
static void put_char(char c) {
    vga_putchar_raw(c);
    debug_log_putchar(c);
    // Queue to serial ring buffer — returns instantly, no port I/O
    if (debug_log_is_enabled()) {
        serial_putchar(c);
    }
}

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
    
    // Calculate how many padding chars we need
    int num_len = i;
    int pad_count = (width > num_len) ? (width - num_len) : 0;
    
    // Print padding first (left-pad)
    for (int j = 0; j < pad_count; j++) {
        put_char(pad);
    }
    
    // Then print the number (reversed from buffer)
    while (i > 0) put_char(buf[--i]);
}

void printf(const char *format, ...) {
    va_list args;
    va_start(args, format);

    while (*format) {
        if (*format == '%') {
            format++;
            
            // Parse width and padding
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
    // Cursor update deferred — call vga_flush() explicitly when needed
}
