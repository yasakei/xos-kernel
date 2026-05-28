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

#include "pit.h"
#include "../../idt.h"
#include "../../lib/printf.h"
#include "../../lib/debuglog.h"
#include "../../drivers/display/vga.h"
#include "../../sched/scheduler.h"
#include "../../drivers/serial/serial.h"

// pit i/o ports
#define PIT_CHANNEL0    0x40    // channel 0 data port (irq0)
#define PIT_COMMAND     0x43    // mode/command register

// pit base frequency (hz) - fixed by hardware
#define PIT_BASE_FREQ   1193182

static volatile uint64_t pit_ticks = 0;
static uint32_t pit_frequency = 0;

// writes a byte to a pit port
static void outb_pit(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

// irq0 handler - fires at the configured frequency
static void pit_irq_handler(struct registers *regs) {
    (void)regs;
    pit_ticks++;
    serial_flush();       // drain serial tx buffer
    scheduler_tick();
}

// sets up the pit to fire at the given frequency and registers the irq handler
void pit_init(uint32_t frequency_hz) {
    pit_frequency = frequency_hz;

    // calculate divisor: pit_base_freq / desired_freq
    uint32_t divisor = PIT_BASE_FREQ / frequency_hz;
    if (divisor > 0xFFFF) divisor = 0xFFFF;
    if (divisor < 1)      divisor = 1;

    // command: channel 0, lobyte/hibyte, mode 3 (square wave)
    outb_pit(PIT_COMMAND, 0x36);

    // send divisor low byte then high byte
    outb_pit(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));
    outb_pit(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xFF));

    // register irq0 handler (idt entry 32)
    register_interrupt_handler(32, pit_irq_handler);

    if (debug_print_is_enabled()) {
        uint8_t prev = vga_get_color();
        vga_set_color(14, 0);
        printf("[PIT] Timer initialized at %d Hz (divisor=%d)\n", frequency_hz, divisor);
        vga_set_color(prev & 0x0F, (prev >> 4) & 0x0F);
    }
}

// returns the total number of pit ticks since boot
uint64_t pit_get_ticks(void) {
    return pit_ticks;
}

// busy-waits for approximately the given number of milliseconds
void pit_sleep_ms(uint32_t ms) {
    uint64_t target = pit_ticks + ((uint64_t)ms * pit_frequency / 1000);
    while (pit_ticks < target) {
        __asm__ volatile("hlt");
    }
}
