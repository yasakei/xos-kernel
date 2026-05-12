#ifndef USERMODE_H
#define USERMODE_H

#include <stdint.h>

// Drop to Ring-3 and jump to user_entry with user_stack as the stack.
// This function never returns.
void usermode_enter(uint64_t user_entry, uint64_t user_stack);

#endif
