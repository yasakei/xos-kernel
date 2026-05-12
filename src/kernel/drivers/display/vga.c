#include "vga.h"

#define VGA_WIDTH  80
#define VGA_HEIGHT 25
#define VGA_MEMORY ((uint16_t*)0xB8000)

static size_t   vga_row;
static size_t   vga_column;
static uint8_t  vga_color;
static uint16_t *vga_buffer;

extern void outb(uint16_t port, uint8_t val);

static void vga_update_cursor(void) {
    uint16_t pos = (uint16_t)(vga_row * VGA_WIDTH + vga_column);
    outb(0x3D4, 0x0F); outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E); outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

void vga_init(void) {
    vga_row    = 0;
    vga_column = 0;
    vga_color  = 7 | (0 << 4);
    vga_buffer = VGA_MEMORY;
    vga_clear();
}

void vga_set_color(uint8_t fg, uint8_t bg) {
    vga_color = fg | (bg << 4);
}

uint8_t vga_get_color(void) {
    return vga_color;
}

void vga_clear(void) {
    for (size_t y = 0; y < VGA_HEIGHT; y++)
        for (size_t x = 0; x < VGA_WIDTH; x++)
            vga_buffer[y * VGA_WIDTH + x] = ((uint16_t)' ') | ((uint16_t)vga_color << 8);
    vga_row    = 0;
    vga_column = 0;
    vga_update_cursor();
}

static void vga_scroll(void) {
    for (size_t y = 1; y < VGA_HEIGHT; y++)
        for (size_t x = 0; x < VGA_WIDTH; x++)
            vga_buffer[(y-1) * VGA_WIDTH + x] = vga_buffer[y * VGA_WIDTH + x];
    for (size_t x = 0; x < VGA_WIDTH; x++)
        vga_buffer[(VGA_HEIGHT-1) * VGA_WIDTH + x] = ((uint16_t)' ') | ((uint16_t)vga_color << 8);
}

// Write char to VGA buffer only — no cursor update
void vga_putchar_raw(char c) {
    if (c == '\n') {
        vga_column = 0;
        vga_row++;
    } else if (c == '\b') {
        if (vga_column > 0) {
            vga_column--;
        } else if (vga_row > 0) {
            vga_row--;
            vga_column = VGA_WIDTH - 1;
        }
        vga_buffer[vga_row * VGA_WIDTH + vga_column] =
            ((uint16_t)' ') | ((uint16_t)vga_color << 8);
    } else {
        vga_buffer[vga_row * VGA_WIDTH + vga_column] =
            ((uint16_t)c) | ((uint16_t)vga_color << 8);
        if (++vga_column == VGA_WIDTH) {
            vga_column = 0;
            vga_row++;
        }
    }
    if (vga_row >= VGA_HEIGHT) {
        vga_row = VGA_HEIGHT - 1;
        vga_scroll();
    }
}

// Write char + update cursor (interactive use only)
void vga_putchar(char c) {
    vga_putchar_raw(c);
    vga_update_cursor();
}

// Write string + update cursor once at end
void vga_write(const char *str) {
    for (size_t i = 0; str[i]; i++)
        vga_putchar_raw(str[i]);
    vga_update_cursor();
}

// Flush cursor to hardware after a batch of vga_putchar_raw calls
void vga_flush(void) {
    vga_update_cursor();
}
