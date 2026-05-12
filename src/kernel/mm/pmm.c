#include "pmm.h"
#include "../lib/printf.h"
#include "../lib/debuglog.h"
#include "../drivers/display/vga.h"

// Target: 256 MB Total RAM / 4KB Page Size = 65536 Base Pages
#define TOTAL_PAGES 65536
#define PAGE_SIZE 4096

// Native 1-bit-per-page Tracking Array (8 KB size in total)
static uint8_t pmm_bitmap[TOTAL_PAGES / 8];

static inline void bitmap_set(size_t bit) {
    pmm_bitmap[bit / 8] |= (1 << (bit % 8));
}

static inline void bitmap_clear(size_t bit) {
    pmm_bitmap[bit / 8] &= ~(1 << (bit % 8));
}

static inline int bitmap_test(size_t bit) {
    return pmm_bitmap[bit / 8] & (1 << (bit % 8));
}

void pmm_init(void) {
    // 1. Defensively mark 100% of memory as IN USE (1) so we don't accidentally allocate critical kernel sectors!
    for (size_t i = 0; i < sizeof(pmm_bitmap); i++) {
        pmm_bitmap[i] = 0xFF;
    }
    
    // 2. We safely assume RAM from 2MB to 256MB is entirely empty in QEMU!
    // 2MB = 0x200000. 0x200000 / 4096 = Page 512.
    // So seamlessly map pages 512 to 65535 as officially FREE (0) natively!
    for (size_t i = 512; i < TOTAL_PAGES; i++) {
        bitmap_clear(i);
    }
    
    if (debug_print_is_enabled()) {
        uint8_t prev = vga_get_color();
        vga_set_color(14, 0);
        printf("[PMM] Physical Memory Allocator ONLINE! Available Pool: ~254 MB\n");
        vga_set_color(prev & 0x0F, (prev >> 4) & 0x0F);
    }
}

void* pmm_alloc_page(void) {
    // Scan sequentially for the lowest available free page block natively
    for (size_t i = 512; i < TOTAL_PAGES; i++) {
        if (!bitmap_test(i)) {
            bitmap_set(i); // Mark tracked frame as active
            uint64_t physical_addr = i * PAGE_SIZE;
            return (void*)physical_addr;
        }
    }
    printf("[PMM] CRITICAL ERROR: OUT OF PHYSICAL MEMORY!\n");
    return NULL;
}

void pmm_free_page(void* ptr) {
    uint64_t physical_addr = (uint64_t)ptr;
    size_t page_idx = physical_addr / PAGE_SIZE;
    
    // Ensure we strictly don't deallocate protected base memory (< 512)
    if (page_idx >= 512 && page_idx < TOTAL_PAGES) {
        bitmap_clear(page_idx);
    }
}

void pmm_get_stats(pmm_stats_t *stats) {
    size_t used = 0;
    for (size_t i = 512; i < TOTAL_PAGES; i++) {
        if (bitmap_test(i)) used++;
    }
    stats->total_pages     = TOTAL_PAGES;
    stats->reserved_pages  = 512;                    // 0–2MB reserved
    stats->used_pages      = used;
    stats->free_pages      = (TOTAL_PAGES - 512) - used;
    stats->page_size       = PAGE_SIZE;
}
