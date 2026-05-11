#include "printf.h"
#include "debuglog.h"
#include "../drivers/display/vga.h"
#include "../drivers/serial/serial.h"
#include <stdarg.h>

static void print_int(long val, int base) {
    char buf[64];
    int i = 0;
    int is_neg = 0;
    
    if (val == 0) {
        debug_log_putchar('0');
        vga_putchar('0');
        serial_putchar('0');
        return;
    }
    
    if (base == 10 && val < 0) {
        is_neg = 1;
        val = -val;
    }
    
    while (val != 0) {
        int rem = val % base;
        buf[i++] = (rem > 9) ? (rem - 10) + 'A' : rem + '0';
        val = val / base;
    }
    
    if (is_neg) {
        buf[i++] = '-';
    }
    
    while (i > 0) {
        i--;
        debug_log_putchar(buf[i]);
        vga_putchar(buf[i]);
        serial_putchar(buf[i]);
    }
}

void printf(const char* format, ...) {
    va_list args;
    va_start(args, format);

    while (*format != '\0') {
        if (*format == '%') {
            format++;
            switch (*format) {
                case 's': {
                    const char* s = va_arg(args, const char*);
                    debug_log_write(s);
                    vga_write(s);
                    serial_write(s);
                    break;
                }
                case 'd': {
                    int val = va_arg(args, int);
                    print_int(val, 10);
                    break;
                }
                case 'x': {
                    unsigned int val = va_arg(args, unsigned int);
                    print_int(val, 16);
                    break;
                }
                case 'p': {
                    long val = (long)va_arg(args, void*);
                    debug_log_write("0x");
                    vga_write("0x");
                    serial_write("0x");
                    print_int(val, 16);
                    break;
                }
                case 'c': {
                    char c = (char)va_arg(args, int);
                    debug_log_putchar(c);
                    vga_putchar(c);
                    serial_putchar(c);
                    break;
                }
                case '%': {
                    debug_log_putchar('%');
                    vga_putchar('%');
                    serial_putchar('%');
                    break;
                }
            }
        } else {
            debug_log_putchar(*format);
            vga_putchar(*format);
            serial_putchar(*format);
        }
        format++;
    }
    
    va_end(args);
}
