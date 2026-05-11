#include "keyboard.h"
#include "idt.h"
#include "printf.h"
#include <stdint.h>

static inline uint8_t inb_kb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ( "inb %1, %0" : "=a"(ret) : "Nd"(port) );
    return ret;
}

// Basic map for US QWERTY lowercase Layout
static const char kbdus[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',   
  '\t', /* Tab */
  'q', 'w', 'e', 'r',   /* 19 */
  't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', /* Enter key */
    0,          /* 29   - Control */
  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', /* 39 */
 '\'', '`',   0,        /* Left shift */
 '\\', 'z', 'x', 'c', 'v', 'b', 'n',          /* 49 */
  'm', ',', '.', '/',   0,              /* Right shift */
  '*',
    0,  /* Alt */
  ' ',  /* Space bar */
    0,  /* Caps lock */
    0,  /* 59 - F1 key ... > */
    0,   0,   0,   0,   0,   0,   0,   0,
    0,  /* < ... F10 */
    0,  /* 69 - Num lock*/
    0,  /* Scroll Lock */
    0,  /* Home key */
    0,  /* Up Arrow */
    0,  /* Page Up */
  '-',
    0,  /* Left Arrow */
    0,
    0,  /* Right Arrow */
  '+',
    0,  /* 79 - End key*/
    0,  /* Down Arrow */
    0,  /* Page Down */
    0,  /* Insert Key */
    0,  /* Delete Key */
    0,   0,   0,
    0,  /* F11 Key */
    0,  /* F12 Key */
    0,  /* All other keys are undefined */
};

static void keyboard_callback(struct registers* regs) {
    (void)regs; // Unused attribute safe
    uint8_t scancode = inb_kb(0x60); // Standard PS/2 Keyboard Port
    
    // The highest bit of the scancode is 1 if the key was released
    if (scancode & 0x80) {
        // Key release - ignore for now
    } else {
        // Key press
        if (scancode < 128) {
            char c = kbdus[scancode];
            if (c != 0) {
                printf("%c", c);
            }
        }
    }
}

void keyboard_init(void) {
    // IRQ1 is the PC Keyboard Controller. Mapped to offset 32 -> Interrupt 33
    register_interrupt_handler(33, keyboard_callback);
}
