#include "vga.h"

#define VGA_WIDTH  80
#define VGA_HEIGHT 25
#define VGA_MEMORY ((uint16_t*)0xB8000)
#define SCROLLBACK_LINES 500  // Keep 500 lines of history

static size_t   vga_row;
static size_t   vga_column;
static uint8_t  vga_color;
static uint16_t *vga_buffer;

// Scrollback buffer
static uint16_t scrollback_buffer[SCROLLBACK_LINES * VGA_WIDTH];
static int scrollback_pos = 0;  // Current write position in circular buffer
static int scrollback_view_offset = 0;  // 0 = viewing live output, >0 = scrolled back
static int scrollback_total_lines = 0;  // Total lines written (capped at SCROLLBACK_LINES)

// Scrollback buffer to save current screen state
static uint16_t current_screen[VGA_HEIGHT * VGA_WIDTH];
static int screen_saved = 0;

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
    scrollback_pos = 0;
    scrollback_view_offset = 0;
    scrollback_total_lines = 0;
    screen_saved = 0;
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
    
    // Clear scrollback buffer
    scrollback_pos = 0;
    scrollback_view_offset = 0;
    scrollback_total_lines = 0;
    
    vga_update_cursor();
}

static void vga_scroll(void) {
    // Save the top line to scrollback buffer before scrolling
    int line_idx = scrollback_pos % SCROLLBACK_LINES;
    for (size_t x = 0; x < VGA_WIDTH; x++) {
        scrollback_buffer[line_idx * VGA_WIDTH + x] = vga_buffer[x];
    }
    scrollback_pos++;
    if (scrollback_total_lines < SCROLLBACK_LINES) {
        scrollback_total_lines++;
    }
    
    // Scroll the screen up
    for (size_t y = 1; y < VGA_HEIGHT; y++)
        for (size_t x = 0; x < VGA_WIDTH; x++)
            vga_buffer[(y-1) * VGA_WIDTH + x] = vga_buffer[y * VGA_WIDTH + x];
    for (size_t x = 0; x < VGA_WIDTH; x++)
        vga_buffer[(VGA_HEIGHT-1) * VGA_WIDTH + x] = ((uint16_t)' ') | ((uint16_t)vga_color << 8);
}

// Write char to VGA buffer only — no cursor update
void vga_putchar_raw(char c) {
    // If user is viewing scrollback, return to live view on new output
    if (scrollback_view_offset > 0) {
        vga_scrollback_reset();
    }
    
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

// Scrollback functions
void vga_scrollback_up(int lines) {
    if (scrollback_total_lines == 0) return;
    
    // Save current screen on first scroll
    if (scrollback_view_offset == 0) {
        for (int i = 0; i < VGA_HEIGHT * VGA_WIDTH; i++) {
            current_screen[i] = vga_buffer[i];
        }
        screen_saved = 1;
    }
    
    scrollback_view_offset += lines;
    if (scrollback_view_offset > scrollback_total_lines) {
        scrollback_view_offset = scrollback_total_lines;
    }
    
    vga_scrollback_render();
}

void vga_scrollback_down(int lines) {
    if (scrollback_view_offset == 0) return;
    
    scrollback_view_offset -= lines;
    if (scrollback_view_offset < 0) {
        scrollback_view_offset = 0;
    }
    
    if (scrollback_view_offset == 0) {
        // Restore the saved screen
        if (screen_saved) {
            for (int i = 0; i < VGA_HEIGHT * VGA_WIDTH; i++) {
                vga_buffer[i] = current_screen[i];
            }
            screen_saved = 0;
        }
        vga_update_cursor();
    } else {
        vga_scrollback_render();
    }
}

void vga_scrollback_reset(void) {
    if (scrollback_view_offset == 0) return;
    
    scrollback_view_offset = 0;
    
    // Restore the saved screen
    if (screen_saved) {
        for (int i = 0; i < VGA_HEIGHT * VGA_WIDTH; i++) {
            vga_buffer[i] = current_screen[i];
        }
        screen_saved = 0;
    }
    
    vga_update_cursor();
}

int vga_scrollback_is_active(void) {
    return scrollback_view_offset > 0;
}

void vga_scrollback_render(void) {
    if (scrollback_view_offset == 0) {
        // Just update cursor to current position
        vga_update_cursor();
        return;
    }
    
    // Calculate which lines to display
    // scrollback_view_offset = how many lines back from current
    int start_line = scrollback_pos - scrollback_view_offset - VGA_HEIGHT;
    
    // Render from scrollback buffer
    for (int y = 0; y < VGA_HEIGHT; y++) {
        int line_idx = (start_line + y);
        if (line_idx < 0) {
            // Before scrollback starts, show blank
            for (size_t x = 0; x < VGA_WIDTH; x++) {
                vga_buffer[y * VGA_WIDTH + x] = ((uint16_t)' ') | ((uint16_t)vga_color << 8);
            }
        } else if (line_idx < scrollback_pos) {
            // Show from scrollback buffer
            int buf_line = line_idx % SCROLLBACK_LINES;
            for (size_t x = 0; x < VGA_WIDTH; x++) {
                vga_buffer[y * VGA_WIDTH + x] = scrollback_buffer[buf_line * VGA_WIDTH + x];
            }
        } else {
            // Show current screen content (shouldn't happen when scrolled back)
            for (size_t x = 0; x < VGA_WIDTH; x++) {
                vga_buffer[y * VGA_WIDTH + x] = ((uint16_t)' ') | ((uint16_t)vga_color << 8);
            }
        }
    }
    
    // Hide cursor when viewing scrollback
    outb(0x3D4, 0x0A);
    outb(0x3D5, 0x20);  // Disable cursor
}
