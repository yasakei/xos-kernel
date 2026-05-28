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

#include "keyboard.h"
#include "../../idt.h"
#include "../../lib/printf.h"
#include <stdint.h>

// temporary serial helper (uses the existing serial driver)
#include "../serial/serial.h"

// reads a byte from a port
static inline uint8_t inb_kb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// us qwerty scancode -> ascii table (unshifted)
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

// us qwerty scancode -> ascii table (shift held down)
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

// modifier key state tracking
static volatile int shift_held = 0;
static volatile int extended = 0;
static volatile int ctrl_held = 0;
static volatile int interrupt_requested = 0;

// internal character queue for buffering scancodes
#define KBD_QUEUE_SIZE 64
static volatile char kbd_queue[KBD_QUEUE_SIZE];
static volatile uint32_t kbd_q_head = 0;
static volatile uint32_t kbd_q_tail = 0;

// special key codes (above ascii range) for arrow keys etc
#define KEY_UP    0x80
#define KEY_DOWN  0x81
#define KEY_LEFT  0x82
#define KEY_RIGHT 0x83
#define KEY_PGUP  0x84
#define KEY_PGDN  0x85
#define KEY_TAB   '\t'

// adds a character to the internal queue (drops if full)
static void kbd_enqueue(char c) {
    uint32_t next = (kbd_q_head + 1U) % KBD_QUEUE_SIZE;
    if (next == kbd_q_tail) {
        return;
    }
    kbd_queue[kbd_q_head] = c;
    kbd_q_head = next;
}

// marks that ctrl+c was received
static void kbd_mark_interrupt_if_needed(char c) {
    if (c == 3) {
        interrupt_requested = 1;
    }
}

// pulls a character from the internal queue, returns 0 if empty
static int kbd_dequeue(char *out) {
    if (kbd_q_tail == kbd_q_head) {
        return 0;
    }
    *out = kbd_queue[kbd_q_tail];
    kbd_q_tail = (kbd_q_tail + 1U) % KBD_QUEUE_SIZE;
    return 1;
}

// turns a raw ps/2 scancode into a character and pushes it into the queue
static void kbd_process_scancode(uint8_t sc) {
    if (sc == 0xE0) {
        extended = 1;
        return;
    }

    // modifier keys: shift and ctrl (left)
    if (sc == 0x2A || sc == 0x36) { shift_held = 1; return; }
    if (sc == 0xAA || sc == 0xB6) { shift_held = 0; return; }
    if (sc == 0x1D) { /* left ctrl pressed */ ctrl_held = 1; return; }
    if (sc == 0x9D) { /* left ctrl released */ ctrl_held = 0; return; }

    if (extended) {
        extended = 0;
        // handle extended ctrl (right ctrl) press/release: e0 1d / e0 9d
        if (sc == 0x1D) { /* right ctrl pressed (extended) */
            ctrl_held = 1;
            return;
        }
        if (sc == 0x9D) { /* right ctrl released (extended) */
            ctrl_held = 0;
            return;
        }

        if (sc & 0x80) return;

        switch (sc) {
            case 0x48: kbd_enqueue((char)KEY_UP); return;
            case 0x50: kbd_enqueue((char)KEY_DOWN); return;
            case 0x4B: kbd_enqueue((char)KEY_LEFT); return;
            case 0x4D: kbd_enqueue((char)KEY_RIGHT); return;
            case 0x49: kbd_enqueue((char)KEY_PGUP); return;
            case 0x51: kbd_enqueue((char)KEY_PGDN); return;
            default: return;
        }
    }

    if (sc & 0x80) return;
    if (sc >= 128) return;



    char c = shift_held ? kbdus_shift[sc] : kbdus[sc];
    if (c) {
        // if ctrl is held and this is a letter, map to control code (e.g. ctrl+c -> 0x03)
        if (ctrl_held) {
            if (c >= 'a' && c <= 'z') {
                c = (char)(c - 'a' + 1); // ctrl+letter
            } else if (c >= 'A' && c <= 'Z') {
                c = (char)(c - 'A' + 1);
            }
        }
        kbd_mark_interrupt_if_needed(c);
        kbd_enqueue(c);
    }
}

// irq1 handler - captures raw scancodes into the internal queue
static void keyboard_callback(struct registers *regs) {
    (void)regs;
    if (inb_kb(0x64) & 0x01) {
        uint8_t sc = inb_kb(0x60);
        kbd_process_scancode(sc);
    }
}

// registers the keyboard interrupt handler
void keyboard_init(void) {
    register_interrupt_handler(33, keyboard_callback);
}

// blocking getchar: polls ps/2 and serial, returns the first available character
char keyboard_getchar(void) {
    while (1) {
        char queued;
        if (kbd_dequeue(&queued)) {
            return queued;
        }
        // if interrupts are disabled, fall back to polling ps/2 directly.
        // when interrupts are enabled we rely on the irq handler to enqueue
        // scancodes to avoid double-processing the same scancode.
        uint64_t rflags;
        __asm__ volatile("pushfq; pop %0" : "=r"(rflags));
        if (!(rflags & (1UL << 9))) {
            // interrupts disabled -> poll ps/2 controller
            if (inb_kb(0x64) & 0x01) {
                uint8_t sc = inb_kb(0x60);
                kbd_process_scancode(sc);
                if (kbd_dequeue(&queued)) {
                    return queued;
                }
                continue;
            }
        }

        // --- serial / com1 (serial mode: ./run.sh serial) ---
        if (inb_kb(0x3FD) & 0x01) {
            char c = (char)inb_kb(0x3F8);
            if (c == '\r') c = '\n';
            if (c == 127)  c = '\b';   // del -> backspace

            kbd_mark_interrupt_if_needed(c);
            
            // handle ansi escape sequences from serial
            if (c == 27) {  // esc
                // check for escape sequence
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

// returns 1 if a character is available in the buffer or from ps/2/serial
int keyboard_has_char(void) {
    return (kbd_q_head != kbd_q_tail) || (inb_kb(0x64) & 0x01) || (inb_kb(0x3FD) & 0x01);
}

// returns 1 if ctrl+c was pressed (used to interrupt long-running commands like ping)
int keyboard_interrupt_requested(void) {
    if (interrupt_requested) {
        return 1;
    }

    // serial-mode ctrl+c can arrive while ping is running; poll it here so
    // long-running commands can notice it without waiting for readline()
    if (inb_kb(0x3FD) & 0x01) {
        char c = (char)inb_kb(0x3F8);
        if (c == '\r') c = '\n';
        if (c == 3) {
            interrupt_requested = 1;
            return 1;
        }
    }

    return 0;
}

// clears the ctrl+c interrupt flag
void keyboard_clear_interrupt_requested(void) {
    interrupt_requested = 0;
}
