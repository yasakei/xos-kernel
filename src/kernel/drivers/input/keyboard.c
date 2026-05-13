#include "keyboard.h"
#include "../../idt.h"
#include "../../lib/printf.h"
#include <stdint.h>

static inline uint8_t inb_kb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// US QWERTY scancode -> ASCII (unshifted)
static const char kbdus[128] = {
      0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
      0,  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
      0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',
      0,  '*',   0, ' ',   0,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,  '-',  0,   0,   0,  '+',
      0,   0,   0,   0,   0,   0,   0,   0,   0,
};

static const char kbdus_shift[128] = {
      0,  27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
      0,  'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
      0,  '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?',
      0,  '*',   0, ' ',   0,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,  '-',  0,   0,   0,  '+',
      0,   0,   0,   0,   0,   0,   0,   0,   0,
};

static volatile int shift_held = 0;
static volatile int extended = 0;

// Special key codes (above ASCII range)
#define KEY_UP    0x80
#define KEY_DOWN  0x81
#define KEY_LEFT  0x82
#define KEY_RIGHT 0x83
#define KEY_PGUP  0x84
#define KEY_PGDN  0x85

// IRQ1 handler — not used for input, just ACKs
static void keyboard_callback(struct registers *regs) {
    (void)regs;
    // Read and discard — we poll directly
    inb_kb(0x60);
}

void keyboard_init(void) {
    register_interrupt_handler(33, keyboard_callback);
}

// Blocking getchar: polls PS/2 and serial, returns first available char.
// Pure busy-poll — no hlt, no yield, no interrupts needed.
char keyboard_getchar(void) {
    while (1) {
        // --- PS/2 keyboard (graphical mode) ---
        // Bit 0 of port 0x64 = output buffer full
        if (inb_kb(0x64) & 0x01) {
            uint8_t sc = inb_kb(0x60);

            // Handle extended scancode prefix
            if (sc == 0xE0) {
                extended = 1;
                continue;
            }

            // Track shift
            if (sc == 0x2A || sc == 0x36) { shift_held = 1; continue; }
            if (sc == 0xAA || sc == 0xB6) { shift_held = 0; continue; }

            // Handle extended keys (arrow keys) BEFORE checking bit 7
            if (extended) {
                extended = 0;
                // Ignore release codes for extended keys too
                if (sc & 0x80) continue;
                
                switch (sc) {
                    case 0x48: return (char)KEY_UP;      // Up arrow
                    case 0x50: return (char)KEY_DOWN;    // Down arrow
                    case 0x4B: return (char)KEY_LEFT;    // Left arrow
                    case 0x4D: return (char)KEY_RIGHT;   // Right arrow
                    case 0x49: return (char)KEY_PGUP;    // Page Up
                    case 0x51: return (char)KEY_PGDN;    // Page Down
                    default: continue;
                }
            }

            // Ignore key releases (bit 7 set)
            if (sc & 0x80) continue;
            if (sc >= 128) continue;

            char c = shift_held ? kbdus_shift[sc] : kbdus[sc];
            if (c) return c;
            continue;
        }

        // --- Serial / COM1 (serial mode: ./run.sh serial) ---
        if (inb_kb(0x3FD) & 0x01) {
            char c = (char)inb_kb(0x3F8);
            if (c == '\r') c = '\n';
            if (c == 127)  c = '\b';   // DEL -> backspace
            
            // Handle ANSI escape sequences from serial
            if (c == 27) {  // ESC
                // Check for escape sequence
                if (inb_kb(0x3FD) & 0x01) {
                    char next = (char)inb_kb(0x3F8);
                    if (next == '[') {
                        if (inb_kb(0x3FD) & 0x01) {
                            char arrow = (char)inb_kb(0x3F8);
                            switch (arrow) {
                                case 'A': return (char)KEY_UP;
                                case 'B': return (char)KEY_DOWN;
                                case 'C': return (char)KEY_RIGHT;
                                case 'D': return (char)KEY_LEFT;
                            }
                        }
                    }
                }
            }
            return c;
        }
    }
}

int keyboard_has_char(void) {
    return (inb_kb(0x64) & 0x01) || (inb_kb(0x3FD) & 0x01);
}
