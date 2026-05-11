#include "idt.h"

// The 32 exception ISRs defined in assembly
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

// Defined in assembly to load IDTR
extern void load_idt(void*);

struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  flags;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

struct idt_entry idt[256];
struct idt_ptr idtr;

static void set_idt_gate(int n, void* handler) {
    uint64_t addr = (uint64_t)handler;
    idt[n].offset_low = addr & 0xFFFF;
    idt[n].selector = 0x08; // 64-bit code segment from our GDT
    idt[n].ist = 0;
    idt[n].flags = 0x8E;    // Present, Ring 0, Interrupt Gate
    idt[n].offset_mid = (addr >> 16) & 0xFFFF;
    idt[n].offset_high = (addr >> 32) & 0xFFFFFFFF;
    idt[n].zero = 0;
}

isr_t interrupt_handlers[256];

void register_interrupt_handler(uint8_t n, isr_t handler) {
    interrupt_handlers[n] = handler;
}

static inline void outb_pic(uint16_t port, uint8_t val) {
    __asm__ volatile ( "outb %0, %1" : : "a"(val), "Nd"(port) );
}

static inline void io_wait(void) {
    outb_pic(0x80, 0);
}

void idt_init(void) {
    idtr.base = (uint64_t)&idt[0];
    idtr.limit = sizeof(struct idt_entry) * 256 - 1;

    for(int i = 0; i < 256; ++i) {
        interrupt_handlers[i] = 0;
    }

    // Set up standard exception gates for fault handling
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

    // Map IRQs 0-15 to IDT entries 32-47
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

    // Remap PIC sequentially with standard hardware I/O delays
    outb_pic(0x20, 0x11); io_wait();
    outb_pic(0xA0, 0x11); io_wait();
    
    outb_pic(0x21, 0x20); io_wait(); // Master bounds to 32
    outb_pic(0xA1, 0x28); io_wait(); // Slave bounds to 40
    
    outb_pic(0x21, 0x04); io_wait(); // Slave attached to IRQ2
    outb_pic(0xA1, 0x02); io_wait(); // Tell Slave its ID
    
    outb_pic(0x21, 0x01); io_wait(); // 8086 mode
    outb_pic(0xA1, 0x01); io_wait(); // 8086 mode

    outb_pic(0x21, 0xEC); io_wait(); // Unmask IRQ0, IRQ1, IRQ4
    outb_pic(0xA1, 0xFF); io_wait(); 

    load_idt(&idtr);
    
    // Enable interrupts
    __asm__ volatile("sti");
}

extern void outb(uint16_t port, uint8_t val);

// C side of the exception fault handler
void interrupt_handler(struct registers* regs) {
    if (interrupt_handlers[regs->int_no] != 0) {
        isr_t handler = interrupt_handlers[regs->int_no];
        handler(regs);
    } else {
        // Unhandled interrupt/fault
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

            // Infinitely halt
            while(1) {
                __asm__ volatile("hlt");
            }
        }
    }

    // Send EOI if it's an IRQ (>= 32 && <= 47)
    if (regs->int_no >= 32 && regs->int_no <= 47) {
        if (regs->int_no >= 40) {
            // Send reset signal to slave
            outb(0xA0, 0x20);
        }
        // Send reset signal to master
        outb(0x20, 0x20);
    }
}
