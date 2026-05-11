#include "pit.h"
#include "../../idt.h"
#include "../../lib/printf.h"
#include "../../sched/scheduler.h"

// PIT I/O ports
#define PIT_CHANNEL0    0x40    // Channel 0 data port (IRQ0)
#define PIT_COMMAND     0x43    // Mode/command register

// PIT base frequency (Hz) — fixed by hardware
#define PIT_BASE_FREQ   1193182

static volatile uint64_t pit_ticks = 0;
static uint32_t pit_frequency = 0;

static void outb_pit(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

// IRQ0 handler — fires at pit_frequency Hz
static void pit_irq_handler(struct registers *regs) {
    (void)regs;
    pit_ticks++;

    // Notify scheduler every tick
    scheduler_tick();
}

void pit_init(uint32_t frequency_hz) {
    pit_frequency = frequency_hz;

    // Calculate divisor: PIT_BASE_FREQ / desired_freq
    uint32_t divisor = PIT_BASE_FREQ / frequency_hz;
    if (divisor > 0xFFFF) divisor = 0xFFFF;
    if (divisor < 1)      divisor = 1;

    // Command: channel 0, lobyte/hibyte, mode 3 (square wave)
    outb_pit(PIT_COMMAND, 0x36);

    // Send divisor low byte then high byte
    outb_pit(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));
    outb_pit(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xFF));

    // Register IRQ0 handler (IDT entry 32)
    register_interrupt_handler(32, pit_irq_handler);

    printf("[PIT] Timer initialized at %d Hz (divisor=%d)\n", frequency_hz, divisor);
}

uint64_t pit_get_ticks(void) {
    return pit_ticks;
}

void pit_sleep_ms(uint32_t ms) {
    uint64_t target = pit_ticks + ((uint64_t)ms * pit_frequency / 1000);
    while (pit_ticks < target) {
        __asm__ volatile("hlt");
    }
}
