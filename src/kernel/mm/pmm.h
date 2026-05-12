#ifndef PMM_H
#define PMM_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    size_t total_pages;     // Total pages in address space
    size_t reserved_pages;  // Pages reserved for kernel/hardware (0-2MB)
    size_t used_pages;      // Allocated pages in the free pool
    size_t free_pages;      // Available pages in the free pool
    size_t page_size;       // Bytes per page
} pmm_stats_t;

void  pmm_init(void);
void* pmm_alloc_page(void);
void  pmm_free_page(void* ptr);
void  pmm_get_stats(pmm_stats_t *stats);

#endif
