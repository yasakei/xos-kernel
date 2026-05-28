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

#include "serial.h"
#include "../../idt.h"
#include <stdint.h>

#define COM1 0x3F8

// tx ring buffer - characters get queued here and sent out whenever possible
#define SERIAL_TX_BUF 4096
static volatile char     tx_buf[SERIAL_TX_BUF];
static volatile uint32_t tx_head = 0;   // read index
static volatile uint32_t tx_tail = 0;   // write index

// checks if the tx ring buffer is empty or full
static inline int tx_empty(void) { return tx_head == tx_tail; }
static inline int tx_full(void)  { return ((tx_tail + 1) % SERIAL_TX_BUF) == tx_head; }

// saves interrupt flags and disables interrupts
static inline uint64_t irq_save_disable(void) {
    uint64_t flags;
    __asm__ volatile("pushfq; pop %0; cli" : "=r"(flags) : : "memory");
    return flags;
}

// restores interrupt flags to what they were before
static inline void irq_restore(uint64_t flags) {
    __asm__ volatile("push %0; popfq" : : "r"(flags) : "memory", "cc");
}

// pushes a character into the tx buffer (drops it if full)
static void tx_push(char c) {
    if (!tx_full()) {
        tx_buf[tx_tail] = c;
        tx_tail = (tx_tail + 1) % SERIAL_TX_BUF;
    }
    // drop if full - serial is just a debug mirror, not critical
}

extern void outb(uint16_t port, uint8_t val);
static uint8_t inb_s(uint16_t port) {
    uint8_t ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// sends as many bytes as the uart will accept right now (non-blocking)
static void serial_drain(void) {
    while (!tx_empty() && (inb_s(COM1 + 5) & 0x20)) {
        outb(COM1, tx_buf[tx_head]);
        tx_head = (tx_head + 1) % SERIAL_TX_BUF;
    }
}

// public api

// sets up the serial port at 115200 baud, 8n1
void serial_init(void) {
    outb(COM1 + 1, 0x00);   // disable interrupts
    outb(COM1 + 3, 0x80);   // enable dlab
    outb(COM1 + 0, 0x01);   // divisor = 1 -> 115200 baud
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);   // 8n1
    outb(COM1 + 2, 0xC7);   // enable + clear fifo, 14-byte threshold
    outb(COM1 + 4, 0x0B);   // rts/dsr
}

// queues a single character - no port i/o, returns instantly
void serial_putchar(char c) {
    uint64_t flags = irq_save_disable();
    tx_push(c);
    // opportunistically drain so early boot messages are visible even
    // before pit is initialized
    serial_drain();
    irq_restore(flags);
}

// queues a string - no port i/o, returns instantly
void serial_write(const char *str) {
    uint64_t flags = irq_save_disable();
    for (int i = 0; str[i]; i++)
        tx_push(str[i]);
    // opportunistically drain so early boot messages are visible even
    // before pit is initialized
    serial_drain();
    irq_restore(flags);
}

// call this periodically (e.g. from the pit handler) to keep the tx buffer draining
void serial_flush(void) {
    uint64_t flags = irq_save_disable();
    serial_drain();
    irq_restore(flags);
}

// returns 1 if there's data ready to read from the serial port
int serial_read_ready(void) {
    return inb_s(COM1 + 5) & 0x01;
}

// reads one byte from the serial port (call only if read_ready returned 1)
char serial_getchar(void) {
    return (char)inb_s(COM1);
}
