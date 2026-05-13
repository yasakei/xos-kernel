#ifndef KEYBOARD_H
#define KEYBOARD_H

// Special key codes (above ASCII range)
#define KEY_UP    0x80
#define KEY_DOWN  0x81
#define KEY_LEFT  0x82
#define KEY_RIGHT 0x83
#define KEY_PGUP  0x84
#define KEY_PGDN  0x85

// Initialize the PS/2 keyboard driver
void keyboard_init(void);

// Read one character from the keyboard buffer (blocking — yields until a key is available)
char keyboard_getchar(void);

// Non-blocking: returns 1 if a character is available in the buffer
int keyboard_has_char(void);

#endif
