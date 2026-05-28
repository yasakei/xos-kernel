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

#include "idt.h"
#include "lib/printf.h"
#include <stdint.h>

// these are the 32 cpu exception handlers, all written in assembly
extern void isr0(void);
extern void isr1(void);
extern void isr2(void);
extern void isr3(void);
extern void isr4(void);
extern void isr5(void);
extern void isr6(void);
extern void isr7(void);
extern void isr8(void);
extern void isr9(void);
extern void isr10(void);
extern void isr11(void);
extern void isr12(void);
extern void isr13(void);
extern void isr14(void);
extern void isr15(void);
extern void isr16(void);
extern void isr17(void);
extern void isr18(void);
extern void isr19(void);
extern void isr20(void);
extern void isr21(void);
extern void isr22(void);
extern void isr23(void);
extern void isr24(void);
extern void isr25(void);
extern void isr26(void);
extern void isr27(void);
extern void isr28(void);
extern void isr29(void);
extern void isr30(void);
extern void isr31(void);

// irq handlers from assembly — the pic fires these
extern void irq0(void);
extern void irq1(void);
extern void irq2(void);
extern void irq3(void);
extern void irq4(void);
extern void irq5(void);
extern void irq6(void);
extern void irq7(void);
extern void irq8(void);
extern void irq9(void);
extern void irq10(void);
extern void irq11(void);
extern void irq12(void);
extern void irq13(void);
extern void irq14(void);
extern void irq15(void);

// loads the idtr register — defined in assembly
extern void load_idt(void*);

// each entry in the interrupt descriptor table
struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  flags;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;
} __attribute__((packed));

// the pointer structure passed to lidt
struct idt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

struct idt_entry idt[256];
struct idt_ptr idtr;

// fill in one idt entry for a given handler
// the handler address is split across three offset fields (64-bit is painful)
static void set_idt_gate(int n, void* handler) {
    uint64_t addr = (uint64_t)handler;
    idt[n].offset_low = addr & 0xFFFF;
    idt[n].selector = 0x08;
    idt[n].ist = 0;
    idt[n].flags = 0x8E;    // present, ring-0, interrupt gate
    idt[n].offset_mid = (addr >> 16) & 0xFFFF;
    idt[n].offset_high = (addr >> 32) & 0xFFFFFFFF;
    idt[n].zero = 0;
}

// same thing but user-callable (dpl=3), used for int 0x80 syscalls
static void set_idt_gate_user(int n, void* handler) {
    uint64_t addr = (uint64_t)handler;
    idt[n].offset_low = addr & 0xFFFF;
    idt[n].selector = 0x08;
    idt[n].ist = 0;
    idt[n].flags = 0xEE;    // present, ring-3 callable, interrupt gate
    idt[n].offset_mid = (addr >> 16) & 0xFFFF;
    idt[n].offset_high = (addr >> 32) & 0xFFFFFFFF;
    idt[n].zero = 0;
}

isr_t interrupt_handlers[256];

// drivers call this to hook a c handler onto a given interrupt vector
void register_interrupt_handler(uint8_t n, isr_t handler) {
    interrupt_handlers[n] = handler;
}

// ok this is the page fault handler — exception 14
// we grab cr2 which holds the faulting address and dump everything useful
static void page_fault_handler(struct registers* regs) {
    extern void serial_flush(void);
    
    uint64_t cr2;
    __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
    
    printf("[PAGE FAULT]\n");
    printf("  Address: %p\n", (void*)cr2);
    printf("  Error code: %p\n", (void*)regs->err_code);
    // decode the error code bits so we know what kind of fault it was
    printf("  Present: %d, Write: %d, User: %d, Reserved: %d, InstructionFetch: %d\n",
        regs->err_code & 1,
        (regs->err_code >> 1) & 1,
        (regs->err_code >> 2) & 1,
        (regs->err_code >> 3) & 1,
        (regs->err_code >> 4) & 1);
    printf("  RIP: %p, CS: %p\n", (void*)regs->rip, (void*)regs->cs);
    printf("  RSP: %p, SS: %p\n", (void*)regs->rsp, (void*)regs->ss);
    printf("[HALT]\n");
    serial_flush();
    
    // no recovery possible — stop everything
    while(1) {
        __asm__ volatile("hlt");
    }
}

static inline void outb_pic(uint16_t port, uint8_t val) {
    __asm__ volatile ( "outb %0, %1" : : "a"(val), "Nd"(port) );
}

// small delay to give the pic time to catch up after an io write
static inline void io_wait(void) {
    outb_pic(0x80, 0);
}

// general protection fault handler — exception 13
// usually means a bad segment selector, a privelege violation, or similar
static void gp_fault_handler(struct registers* regs) {
    extern void serial_flush(void);
    printf("[GP FAULT] Exception 13\n");
    printf("  Error code: %p\n", (void*)regs->err_code);
    printf("  RIP: %p, CS: %p\n", (void*)regs->rip, (void*)regs->cs);
    printf("  RSP: %p, SS: %p\n", (void*)regs->rsp, (void*)regs->ss);
    printf("[HALT]\n");
    serial_flush();
    while(1) { __asm__ volatile("hlt"); }
}

void idt_init(void) {
    idtr.base = (uint64_t)&idt[0];
    idtr.limit = sizeof(struct idt_entry) * 256 - 1;

    // clear out all handler slots first
    for(int i = 0; i < 256; ++i) {
        interrupt_handlers[i] = 0;
    }

    // wire up exception gates 0–31 to their assembly stubs
    // these cover things like divide-by-zero, page fault, gp fault, etc.
    set_idt_gate(0, isr0);
    set_idt_gate(1, isr1);
    set_idt_gate(2, isr2);
    set_idt_gate(3, isr3);
    set_idt_gate(4, isr4);
    set_idt_gate(5, isr5);
    set_idt_gate(6, isr6);
    set_idt_gate(7, isr7);
    set_idt_gate(8, isr8);
    set_idt_gate(9, isr9);
    set_idt_gate(10, isr10);
    set_idt_gate(11, isr11);
    set_idt_gate(12, isr12);
    set_idt_gate(13, isr13);
    set_idt_gate(14, isr14);
    set_idt_gate(15, isr15);
    set_idt_gate(16, isr16);
    set_idt_gate(17, isr17);
    set_idt_gate(18, isr18);
    set_idt_gate(19, isr19);
    set_idt_gate(20, isr20);
    set_idt_gate(21, isr21);
    set_idt_gate(22, isr22);
    set_idt_gate(23, isr23);
    set_idt_gate(24, isr24);
    set_idt_gate(25, isr25);
    set_idt_gate(26, isr26);
    set_idt_gate(27, isr27);
    set_idt_gate(28, isr28);
    set_idt_gate(29, isr29);
    set_idt_gate(30, isr30);
    set_idt_gate(31, isr31);

    // map irqs 0–15 to idt entries 32–47 (standard pc layout)
    set_idt_gate(32, irq0);
    set_idt_gate(33, irq1);
    set_idt_gate(34, irq2);
    set_idt_gate(35, irq3);
    set_idt_gate(36, irq4);
    set_idt_gate(37, irq5);
    set_idt_gate(38, irq6);
    set_idt_gate(39, irq7);
    set_idt_gate(40, irq8);
    set_idt_gate(41, irq9);
    set_idt_gate(42, irq10);
    set_idt_gate(43, irq11);
    set_idt_gate(44, irq12);
    set_idt_gate(45, irq13);
    set_idt_gate(46, irq14);
    set_idt_gate(47, irq15);

    // remap the pic so irqs don't overlap with cpu exceptions
    // the pic uses a legacy protocol: send initialization command, then set offsets
    outb_pic(0x20, 0x11); io_wait();
    outb_pic(0xA0, 0x11); io_wait();
    
    outb_pic(0x21, 0x20); io_wait(); // master starts at irq 32
    outb_pic(0xA1, 0x28); io_wait(); // slave starts at irq 40
    
    outb_pic(0x21, 0x04); io_wait(); // tell master slave is on irq2
    outb_pic(0xA1, 0x02); io_wait(); // tell slave its cascade id
    
    outb_pic(0x21, 0x01); io_wait(); // put master in 8086 mode
    outb_pic(0xA1, 0x01); io_wait(); // put slave in 8086 mode

    // unmask irq0 (timer), irq1 (keyboard), irq4 (serial)
    outb_pic(0x21, 0xEC); io_wait();
    outb_pic(0xA1, 0xFF); io_wait(); 

    extern void isr128(void);

    // int 0x80 syscall gate — dpl=3 so user-mode code can call it
    set_idt_gate_user(0x80, isr128);

    // register our c-level handlers for page faults and gp faults
    register_interrupt_handler(14, page_fault_handler);
    register_interrupt_handler(13, gp_fault_handler);

    // load the idt and actually enable interrupts on the cpu
    load_idt(&idtr);
    
    __asm__ volatile("sti");
}

extern void outb(uint16_t port, uint8_t val);

// the main dispatch function called from the assembly stub
// routes to the registered c handler or panics if unhandled
void interrupt_handler(struct registers* regs) {
    extern void serial_flush(void);
    
    if (interrupt_handlers[regs->int_no] != 0) {
        isr_t handler = interrupt_handlers[regs->int_no];
        handler(regs);
    } else {
        // no one registered for this interrupt — dump the fault number
        if (regs->int_no < 32) {
            outb(0x3F8, 'F');
            outb(0x3F8, 'A');
            outb(0x3F8, 'U');
            outb(0x3F8, 'L');
            outb(0x3F8, 'T');
            outb(0x3F8, ' ');
            if (regs->int_no < 10) outb(0x3F8, '0' + regs->int_no);
            else {
                outb(0x3F8, '0' + (regs->int_no / 10));
                outb(0x3F8, '0' + (regs->int_no % 10));
            }
            outb(0x3F8, '\n');
            serial_flush();

            // unhandled cpu exception is fatal — stop here
            while(1) {
                __asm__ volatile("hlt");
            }
        }
    }

    // send eoi if it was an irq (entries 32–47)
    if (regs->int_no >= 32 && regs->int_no <= 47) {
        if (regs->int_no >= 40) {
            // slave pic needs its own eoi
            outb(0xA0, 0x20);
        }
        // master pic always gets an eoi
        outb(0x20, 0x20);
    }
}
