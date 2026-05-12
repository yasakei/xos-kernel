#include "gdt.h"
#include "../lib/printf.h"
#include "../lib/debuglog.h"
#include "../drivers/display/vga.h"
#include <stdint.h>

// ── Types ─────────────────────────────────────────────────────────────────────

typedef struct {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  gran;
    uint8_t  base_high;
} __attribute__((packed)) gdt_entry_t;

typedef struct {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  gran;
    uint8_t  base_high;
    uint32_t base_upper;
    uint32_t reserved;
} __attribute__((packed)) tss_desc_t;

typedef struct {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist[7];
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iopb_offset;
} __attribute__((packed)) tss_t;

typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) gdtr_t;

// ── Static data ───────────────────────────────────────────────────────────────

// Flat GDT buffer: 5 × 8-byte entries + 1 × 16-byte TSS = 56 bytes
static uint8_t  gdt_buf[56];
static tss_t    tss;
static gdtr_t   gdtr;

// ── Helpers ───────────────────────────────────────────────────────────────────

static void write_entry(int idx, uint32_t base, uint32_t limit,
                        uint8_t access, uint8_t gran) {
    gdt_entry_t e;
    e.limit_low = (uint16_t)(limit & 0xFFFF);
    e.base_low  = (uint16_t)(base  & 0xFFFF);
    e.base_mid  = (uint8_t)((base  >> 16) & 0xFF);
    e.access    = access;
    e.gran      = (uint8_t)(((limit >> 16) & 0x0F) | (gran & 0xF0));
    e.base_high = (uint8_t)((base  >> 24) & 0xFF);
    uint8_t *dst = gdt_buf + idx * 8;
    uint8_t *src = (uint8_t*)&e;
    for (int i = 0; i < 8; i++) dst[i] = src[i];
}

static void write_tss_desc(uint64_t base, uint32_t limit) {
    tss_desc_t d;
    d.limit_low  = (uint16_t)(limit & 0xFFFF);
    d.base_low   = (uint16_t)(base  & 0xFFFF);
    d.base_mid   = (uint8_t)((base  >> 16) & 0xFF);
    d.access     = 0x89;  // present, DPL=0, type=9 (64-bit TSS available)
    d.gran       = (uint8_t)((limit >> 16) & 0x0F);
    d.base_high  = (uint8_t)((base  >> 24) & 0xFF);
    d.base_upper = (uint32_t)(base  >> 32);
    d.reserved   = 0;
    uint8_t *dst = gdt_buf + 5 * 8;  // offset 40
    uint8_t *src = (uint8_t*)&d;
    for (int i = 0; i < 16; i++) dst[i] = src[i];
}

// ── Public API ────────────────────────────────────────────────────────────────

void gdt_init(void) {
    // 0x00: null
    write_entry(0, 0, 0, 0, 0);
    // 0x08: kernel code — Ring-0, 64-bit
    write_entry(1, 0, 0xFFFFF, 0x9A, 0xA0);
    // 0x10: kernel data — Ring-0
    write_entry(2, 0, 0xFFFFF, 0x92, 0xC0);
    // 0x18: user code — Ring-3, 64-bit  (selector 0x1B = 0x18|3)
    write_entry(3, 0, 0xFFFFF, 0xFA, 0xA0);
    // 0x20: user data — Ring-3          (selector 0x23 = 0x20|3)
    write_entry(4, 0, 0xFFFFF, 0xF2, 0xC0);
    // 0x28: TSS (16-byte descriptor)
    uint8_t *tp = (uint8_t*)&tss;
    for (uint32_t i = 0; i < sizeof(tss_t); i++) tp[i] = 0;
    tss.iopb_offset = (uint16_t)sizeof(tss_t);
    write_tss_desc((uint64_t)&tss, (uint32_t)(sizeof(tss_t) - 1));

    // Load GDTR
    gdtr.base  = (uint64_t)gdt_buf;
    gdtr.limit = (uint16_t)(sizeof(gdt_buf) - 1);
    __asm__ volatile("lgdt %0" : : "m"(gdtr));

    // Reload data segments
    __asm__ volatile(
        "mov $0x10, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        "mov %%ax, %%ss\n"
        : : : "ax"
    );

    // Load TSS
    __asm__ volatile("ltr %0" : : "r"((uint16_t)0x28));
    if (debug_print_is_enabled()) {
        uint8_t prev = vga_get_color();
        vga_set_color(14, 0); /* yellow on black */
        printf("[GDT] GDT + TSS loaded OK\n");
        vga_set_color(prev & 0x0F, (prev >> 4) & 0x0F);
    }
}

void tss_set_kernel_stack(uint64_t rsp0) {
    tss.rsp0 = rsp0;
}
