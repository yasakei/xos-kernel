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

#include "vga.h"

#define VGA_WIDTH  80
#define VGA_HEIGHT 25
#define VGA_MEMORY ((uint16_t*)0xB8000)
#define SCROLLBACK_LINES 500  // keep 500 lines of history

static size_t   vga_row;
static size_t   vga_column;
static uint8_t  vga_color;
static uint16_t *vga_buffer;

// scrollback buffer - stores old lines so we can scroll back
static uint16_t scrollback_buffer[SCROLLBACK_LINES * VGA_WIDTH];
static int scrollback_pos = 0;  // current write position in circular buffer
static int scrollback_view_offset = 0;  // 0 = viewing live output, >0 = scrolled back
static int scrollback_total_lines = 0;  // total lines written (capped at scrollback_lines)

// saves current screen state so we can restore it after scrolling back
static uint16_t current_screen[VGA_HEIGHT * VGA_WIDTH];
static int screen_saved = 0;

extern void outb(uint16_t port, uint8_t val);

// updates the hardware cursor position to match the current row/column
static void vga_update_cursor(void) {
    uint16_t pos = (uint16_t)(vga_row * VGA_WIDTH + vga_column);
    outb(0x3D4, 0x0F); outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E); outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

// sets up the vga state and clears the screen
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

// sets the foreground and background colors for future text
void vga_set_color(uint8_t fg, uint8_t bg) {
    vga_color = fg | (bg << 4);
}

// returns the current vga color byte
uint8_t vga_get_color(void) {
    return vga_color;
}

// clears the entire screen and resets the scrollback buffer
void vga_clear(void) {
    for (size_t y = 0; y < VGA_HEIGHT; y++)
        for (size_t x = 0; x < VGA_WIDTH; x++)
            vga_buffer[y * VGA_WIDTH + x] = ((uint16_t)' ') | ((uint16_t)vga_color << 8);
    vga_row    = 0;
    vga_column = 0;
    
    // clear scrollback buffer
    scrollback_pos = 0;
    scrollback_view_offset = 0;
    scrollback_total_lines = 0;
    
    vga_update_cursor();
}

// scrolls the screen up one line and saves the top line in the scrollback buffer
static void vga_scroll(void) {
    // save the top line to scrollback buffer before scrolling
    int line_idx = scrollback_pos % SCROLLBACK_LINES;
    for (size_t x = 0; x < VGA_WIDTH; x++) {
        scrollback_buffer[line_idx * VGA_WIDTH + x] = vga_buffer[x];
    }
    scrollback_pos++;
    if (scrollback_total_lines < SCROLLBACK_LINES) {
        scrollback_total_lines++;
    }
    
    // scroll the screen up
    for (size_t y = 1; y < VGA_HEIGHT; y++)
        for (size_t x = 0; x < VGA_WIDTH; x++)
            vga_buffer[(y-1) * VGA_WIDTH + x] = vga_buffer[y * VGA_WIDTH + x];
    for (size_t x = 0; x < VGA_WIDTH; x++)
        vga_buffer[(VGA_HEIGHT-1) * VGA_WIDTH + x] = ((uint16_t)' ') | ((uint16_t)vga_color << 8);
}

// writes a character to the vga buffer without updating the hardware cursor
void vga_putchar_raw(char c) {
    // if user is viewing scrollback, return to live view on new output
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

// writes a character and updates the hardware cursor (for interactive use)
void vga_putchar(char c) {
    vga_putchar_raw(c);
    vga_update_cursor();
}

// writes a string and updates the cursor once at the end
void vga_write(const char *str) {
    for (size_t i = 0; str[i]; i++)
        vga_putchar_raw(str[i]);
    vga_update_cursor();
}

// flushes the cursor to hardware after a batch of putchar_raw calls
void vga_flush(void) {
    vga_update_cursor();
}

// scrolls the scrollback view up by the given number of lines
void vga_scrollback_up(int lines) {
    if (scrollback_total_lines == 0) return;
    
    // save current screen on first scroll
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

// scrolls the scrollback view down by the given number of lines
void vga_scrollback_down(int lines) {
    if (scrollback_view_offset == 0) return;
    
    scrollback_view_offset -= lines;
    if (scrollback_view_offset < 0) {
        scrollback_view_offset = 0;
    }
    
    if (scrollback_view_offset == 0) {
        // restore the saved screen
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

// resets the scrollback view back to live output
void vga_scrollback_reset(void) {
    if (scrollback_view_offset == 0) return;
    
    scrollback_view_offset = 0;
    
    // restore the saved screen
    if (screen_saved) {
        for (int i = 0; i < VGA_HEIGHT * VGA_WIDTH; i++) {
            vga_buffer[i] = current_screen[i];
        }
        screen_saved = 0;
    }
    
    vga_update_cursor();
}

// returns 1 if the user is currently viewing scrollback history
int vga_scrollback_is_active(void) {
    return scrollback_view_offset > 0;
}

// renders the scrollback history onto the screen
void vga_scrollback_render(void) {
    if (scrollback_view_offset == 0) {
        // just update cursor to current position
        vga_update_cursor();
        return;
    }
    
    // calculate which lines to display
    // scrollback_view_offset = how many lines back from current
    int start_line = scrollback_pos - scrollback_view_offset - VGA_HEIGHT;
    
    // render from scrollback buffer
    for (int y = 0; y < VGA_HEIGHT; y++) {
        int line_idx = (start_line + y);
        if (line_idx < 0) {
            // before scrollback starts, show blank
            for (size_t x = 0; x < VGA_WIDTH; x++) {
                vga_buffer[y * VGA_WIDTH + x] = ((uint16_t)' ') | ((uint16_t)vga_color << 8);
            }
        } else if (line_idx < scrollback_pos) {
            // show from scrollback buffer
            int buf_line = line_idx % SCROLLBACK_LINES;
            for (size_t x = 0; x < VGA_WIDTH; x++) {
                vga_buffer[y * VGA_WIDTH + x] = scrollback_buffer[buf_line * VGA_WIDTH + x];
            }
        } else {
            // show current screen content (shouldn't happen when scrolled back)
            for (size_t x = 0; x < VGA_WIDTH; x++) {
                vga_buffer[y * VGA_WIDTH + x] = ((uint16_t)' ') | ((uint16_t)vga_color << 8);
            }
        }
    }
    
    // hide cursor when viewing scrollback
    outb(0x3D4, 0x0A);
    outb(0x3D5, 0x20);  // disable cursor
}
