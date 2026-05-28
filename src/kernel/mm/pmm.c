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

#include "pmm.h"
#include "../lib/printf.h"
#include "../lib/debuglog.h"
#include "../drivers/display/vga.h"

// we support up to 256 mb total ram, 4kb page size = 65536 pages
#define TOTAL_PAGES 65536
#define PAGE_SIZE 4096

// bitmap with one bit per page, 8 kb total
static uint8_t pmm_bitmap[TOTAL_PAGES / 8];

// mark a page as used in the bitmap
static inline void bitmap_set(size_t bit) {
    pmm_bitmap[bit / 8] |= (1 << (bit % 8));
}

// mark a page as free in the bitmap
static inline void bitmap_clear(size_t bit) {
    pmm_bitmap[bit / 8] &= ~(1 << (bit % 8));
}

// check if a page is used (returns non-zero if set)
static inline int bitmap_test(size_t bit) {
    return pmm_bitmap[bit / 8] & (1 << (bit % 8));
}

// initialize the physical memory manager
void pmm_init(void) {
    // mark everything as used by default so we don't accidentally hand out kernel memory
    for (size_t i = 0; i < sizeof(pmm_bitmap); i++) {
        pmm_bitmap[i] = 0xFF;
    }
    
    // assume ram from 2mb to 256mb is free in qemu
    // 2mb = 0x200000, which is page 512. mark pages 512-65535 as free
    for (size_t i = 512; i < TOTAL_PAGES; i++) {
        bitmap_clear(i);
    }
    
    if (debug_print_is_enabled()) {
        uint8_t prev = vga_get_color();
        vga_set_color(14, 0);
        printf("[PMM] Physical memory allocator ready, ~254 MB available\n");
        vga_set_color(prev & 0x0F, (prev >> 4) & 0x0F);
    }
}

// allocate a single 4kb page from physical memory
void* pmm_alloc_page(void) {
    // scan sequentially for the first free page
    for (size_t i = 512; i < TOTAL_PAGES; i++) {
        if (!bitmap_test(i)) {
            bitmap_set(i);
            uint64_t physical_addr = i * PAGE_SIZE;
            return (void*)physical_addr;
        }
    }
    printf("[PMM] out of physical memory!\n");
    return NULL;
}

// free a previously allocated page
void pmm_free_page(void* ptr) {
    uint64_t physical_addr = (uint64_t)ptr;
    size_t page_idx = physical_addr / PAGE_SIZE;
    
    // don't free pages in the reserved range (below page 512)
    if (page_idx >= 512 && page_idx < TOTAL_PAGES) {
        bitmap_clear(page_idx);
    }
}

// get memory usage statistics
void pmm_get_stats(pmm_stats_t *stats) {
    size_t used = 0;
    for (size_t i = 512; i < TOTAL_PAGES; i++) {
        if (bitmap_test(i)) used++;
    }
    stats->total_pages     = TOTAL_PAGES;
    stats->reserved_pages  = 512;                    // 0-2mb reserved
    stats->used_pages      = used;
    stats->free_pages      = (TOTAL_PAGES - 512) - used;
    stats->page_size       = PAGE_SIZE;
}
