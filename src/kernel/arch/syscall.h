#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>

// Syscall numbers
#define SYS_WRITE   0   // rdi = const char* str
#define SYS_YIELD   1   // no args
#define SYS_EXIT    2   // rdi = exit code

void syscall_init(void);

#endif
