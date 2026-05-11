#ifndef PIT_H
#define PIT_H

#include <stdint.h>

// Initialize PIT to fire at the given frequency (Hz)
void pit_init(uint32_t frequency_hz);

// Get total tick count since boot
uint64_t pit_get_ticks(void);

// Sleep for approximately ms milliseconds (busy-wait)
void pit_sleep_ms(uint32_t ms);

#endif
