#include "serial.h"
#include "idt.h"
#include "printf.h"
#include <stdint.h>

#define COM1 0x3F8

extern void outb(uint16_t port, uint8_t val);

static uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ( "inb %1, %0" : "=a"(ret) : "Nd"(port) );
    return ret;
}

static void serial_callback(struct registers* regs) {
    (void)regs;
    // Data Ready bit is 0x01 on Line Status Register
    while (inb(COM1 + 5) & 1) {
        char c = inb(COM1);
        
        // Output locally back to console terminal to mirror what user types
        printf("%c", c);
    }
}

void serial_init(void) {
    outb(COM1 + 1, 0x00);    // Disable all interrupts instantly
    outb(COM1 + 3, 0x80);    // Enable DLAB 
    outb(COM1 + 0, 0x03);    // Divisor = 3 (38400 baud)
    outb(COM1 + 1, 0x00);    
    outb(COM1 + 3, 0x03);    // 8 bits, no parity, one stop bit
    outb(COM1 + 2, 0xC7);    // Enable FIFO
    outb(COM1 + 4, 0x0B);    // IRQs enabled, RTS/DSR set
    outb(COM1 + 1, 0x01);    // Enable 'Receive Data Available' Line Interrupt securely

    // Register generic UART COM1 IRQ4 (Offset 32 + 4)
    register_interrupt_handler(36, serial_callback);
}

static int serial_is_transmit_empty() {
   return inb(COM1 + 5) & 0x20;
}

void serial_putchar(char c) {
    while (serial_is_transmit_empty() == 0);
    outb(COM1, c);
}

void serial_write(const char* str) {
    for (int i = 0; str[i] != '\0'; i++) {
        serial_putchar(str[i]);
    }
}
