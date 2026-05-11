#include "vga.h"

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_MEMORY ((uint16_t*)0xB8000)

static size_t vga_row;
static size_t vga_column;
static uint8_t vga_color;
static uint16_t* vga_buffer;

extern void outb(uint16_t port, uint8_t val);

static void vga_update_cursor(int x, int y) {
    uint16_t pos = y * VGA_WIDTH + x;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

void vga_init(void) {
    vga_row = 0;
    vga_column = 0;
    vga_color = 7 | (0 << 4); // Light grey on black
    vga_buffer = VGA_MEMORY;
    vga_clear();
}

void vga_set_color(uint8_t fg, uint8_t bg) {
    vga_color = fg | (bg << 4);
}

void vga_clear(void) {
    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            const size_t index = y * VGA_WIDTH + x;
            vga_buffer[index] = ((uint16_t)' ') | ((uint16_t)vga_color << 8);
        }
    }
    vga_row = 0;
    vga_column = 0;
    vga_update_cursor(0, 0);
}

static void vga_scroll(void) {
    for (size_t y = 1; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            vga_buffer[(y - 1) * VGA_WIDTH + x] = vga_buffer[y * VGA_WIDTH + x];
        }
    }
    for (size_t x = 0; x < VGA_WIDTH; x++) {
        vga_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = ((uint16_t)' ') | ((uint16_t)vga_color << 8);
    }
}

void vga_putchar(char c) {
    if (c == '\n') {
        vga_row++;
        vga_column = 0;
    } else {
        const size_t index = vga_row * VGA_WIDTH + vga_column;
        vga_buffer[index] = ((uint16_t)c) | ((uint16_t)vga_color << 8);
        if (++vga_column == VGA_WIDTH) {
            vga_column = 0;
            if (++vga_row == VGA_HEIGHT) {
                vga_row--;
                vga_scroll();
            }
        }
    }

    if (vga_row == VGA_HEIGHT) {
        vga_row--;
        vga_scroll();
    }
    vga_update_cursor(vga_column, vga_row);
}

void vga_write(const char* str) {
    for (size_t i = 0; str[i] != '\0'; i++) {
        vga_putchar(str[i]);
    }
}
