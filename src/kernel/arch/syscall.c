#include "syscall.h"
#include "../idt.h"
#include "../lib/printf.h"
#include "../lib/debuglog.h"
#include "../drivers/display/vga.h"
#include "../sched/scheduler.h"
#include <stdint.h>

// int 0x80 handler — called from Ring-3 via interrupt
static void syscall_handler(struct registers *regs) {
    uint64_t syscall_num = regs->rax;

    switch (syscall_num) {
        case SYS_WRITE: {
            // rdi = pointer to null-terminated string
            const char *str = (const char *)regs->rdi;
            // Basic safety: only print if pointer looks valid
            if (str && (uint64_t)str > 0x1000) {
                printf("%s", str);
            }
            regs->rax = 0;  // return 0 = success
            break;
        }
        case SYS_YIELD: {
            scheduler_yield();
            regs->rax = 0;
            break;
        }
        case SYS_EXIT: {
            printf("[SYSCALL] Task exited with code %d\n", (int)regs->rdi);
            // Mark current task as dead and yield
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
    // Register int 0x80 (IDT entry 0x80 = 128)
    register_interrupt_handler(0x80, syscall_handler);
    if (debug_print_is_enabled()) {
        uint8_t prev = vga_get_color();
        vga_set_color(14, 0);
        printf("[SYSCALL] int 0x80 handler registered\n");
        vga_set_color(prev & 0x0F, (prev >> 4) & 0x0F);
    }
}
