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

#ifndef VGA_H
#define VGA_H

#include <stdint.h>
#include <stddef.h>

// core vga text mode functions
void vga_init(void);
void vga_clear(void);
void vga_putchar(char c);       // write char + update cursor (interactive use)
void vga_putchar_raw(char c);   // write char, no cursor update (batch use)
void vga_write(const char *str);// write string + update cursor once
void vga_flush(void);           // update cursor after batch of putchar_raw calls
void vga_set_color(uint8_t fg, uint8_t bg);
uint8_t vga_get_color(void);

// scrollback functions - lets you see past output
void vga_scrollback_up(int lines);
void vga_scrollback_down(int lines);
void vga_scrollback_reset(void);
int vga_scrollback_is_active(void);
void vga_scrollback_render(void);

#endif
