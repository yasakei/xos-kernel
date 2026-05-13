#ifndef VGA_H
#define VGA_H

#include <stdint.h>
#include <stddef.h>

void vga_init(void);
void vga_clear(void);
void vga_putchar(char c);       // write char + update cursor (interactive use)
void vga_putchar_raw(char c);   // write char, no cursor update (batch use)
void vga_write(const char *str);// write string + update cursor once
void vga_flush(void);           // update cursor after batch of putchar_raw calls
void vga_set_color(uint8_t fg, uint8_t bg);
uint8_t vga_get_color(void);

// Scrollback functions
void vga_scrollback_up(int lines);
void vga_scrollback_down(int lines);
void vga_scrollback_reset(void);
int vga_scrollback_is_active(void);
void vga_scrollback_render(void);

#endif
