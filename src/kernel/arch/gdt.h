#ifndef GDT_H
#define GDT_H

#include <stdint.h>

// Segment selectors
#define GDT_KERNEL_CODE  0x08
#define GDT_KERNEL_DATA  0x10
#define GDT_USER_CODE    0x1B   // 0x18 | RPL=3
#define GDT_USER_DATA    0x23   // 0x20 | RPL=3
#define GDT_TSS          0x28

// Initialize GDT with user segments + TSS, load it
void gdt_init(void);

// Update TSS rsp0 (kernel stack pointer for Ring-3 → Ring-0 transitions)
void tss_set_kernel_stack(uint64_t rsp0);

#endif
