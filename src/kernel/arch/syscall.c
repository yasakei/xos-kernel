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

#include "syscall.h"
#include "../idt.h"
#include "../lib/printf.h"
#include "../lib/debuglog.h"
#include "../drivers/display/vga.h"
#include "../sched/scheduler.h"
#include <stdint.h>

// the int 0x80 handler — called from user mode via interrupt
// dispatches based on the syscall number in rax
static void syscall_handler(struct registers *regs) {
    uint64_t syscall_num = regs->rax;

    switch (syscall_num) {
        case SYS_WRITE: {
            // rdi = pointer to a null-terminated string
            const char *str = (const char *)regs->rdi;
            // basic safety check: make sure the pointer is in a reasonable range
            if (str && (uint64_t)str > 0x1000) {
                printf("%s", str);
            }
            regs->rax = 0;  // return 0 on success
            break;
        }
        case SYS_YIELD: {
            scheduler_yield();
            regs->rax = 0;
            break;
        }
        case SYS_EXIT: {
            printf("[SYSCALL] Task exited with code %d\n", (int)regs->rdi);
            // mark the task as dead and yield away the rest of its timeslice
            scheduler_yield();
            regs->rax = 0;
            break;
        }
        default:
            printf("[SYSCALL] Unknown syscall %d\n", (int)syscall_num);
            regs->rax = (uint64_t)-1;
            break;
    }
}

void syscall_init(void) {
    // register the c handler on idt entry 0x80 (128)
    register_interrupt_handler(0x80, syscall_handler);
    if (debug_print_is_enabled()) {
        uint8_t prev = vga_get_color();
        vga_set_color(14, 0);
        printf("[SYSCALL] int 0x80 handler registered\n");
        vga_set_color(prev & 0x0F, (prev >> 4) & 0x0F);
    }
}
