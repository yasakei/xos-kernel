#ifndef KEYBOARD_H
#define KEYBOARD_H

// Initialize the PS/2 keyboard driver
void keyboard_init(void);

// Read one character from the keyboard buffer (blocking — yields until a key is available)
char keyboard_getchar(void);

// Non-blocking: returns 1 if a character is available in the buffer
int keyboard_has_char(void);

#endif
