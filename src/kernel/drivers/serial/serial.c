#include "serial.h"
#include "../../idt.h"
#include <stdint.h>

#define COM1 0x3F8

// ── TX ring buffer ────────────────────────────────────────────────────────────
// Characters are queued here and drained opportunistically.
// VGA never waits for serial.
#define SERIAL_TX_BUF 4096
static volatile char     tx_buf[SERIAL_TX_BUF];
static volatile uint32_t tx_head = 0;   // read index
static volatile uint32_t tx_tail = 0;   // write index

static inline int tx_empty(void) { return tx_head == tx_tail; }
static inline int tx_full(void)  { return ((tx_tail + 1) % SERIAL_TX_BUF) == tx_head; }

static void tx_push(char c) {
    if (!tx_full()) {
        tx_buf[tx_tail] = c;
        tx_tail = (tx_tail + 1) % SERIAL_TX_BUF;
    }
    // drop if full — serial is just a debug mirror, not critical
}

extern void outb(uint16_t port, uint8_t val);
static uint8_t inb_s(uint16_t port) {
    uint8_t ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// Drain as many bytes as the UART FIFO will accept right now (non-blocking)
static void serial_drain(void) {
    while (!tx_empty() && (inb_s(COM1 + 5) & 0x20)) {
        outb(COM1, tx_buf[tx_head]);
        tx_head = (tx_head + 1) % SERIAL_TX_BUF;
    }
}

// ── Public API ────────────────────────────────────────────────────────────────

void serial_init(void) {
    outb(COM1 + 1, 0x00);   // Disable interrupts
    outb(COM1 + 3, 0x80);   // Enable DLAB
    outb(COM1 + 0, 0x01);   // Divisor = 1 → 115200 baud
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);   // 8N1
    outb(COM1 + 2, 0xC7);   // Enable + clear FIFO, 14-byte threshold
    outb(COM1 + 4, 0x0B);   // RTS/DSR
}

// Queue one char — zero port I/O, returns instantly
void serial_putchar(char c) {
    tx_push(c);
    // Do NOT drain here — PIT handler drains at 100Hz
}

// Queue a string — zero port I/O, returns instantly
void serial_write(const char *str) {
    for (int i = 0; str[i]; i++)
        tx_push(str[i]);
    // Do NOT drain here — PIT handler drains at 100Hz
}

// Call this periodically (e.g. from PIT handler) to keep draining
void serial_flush(void) {
    serial_drain();
}

// Blocking read from serial RX (for shell input)
int serial_read_ready(void) {
    return inb_s(COM1 + 5) & 0x01;
}

char serial_getchar(void) {
    return (char)inb_s(COM1);
}
