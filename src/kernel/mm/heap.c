#include "heap.h"
#include "pmm.h"
#include "../lib/printf.h"
#include <stdint.h>

// A native rapid Sequential Bump-Allocator kernel heap

static uint8_t* heap_cursor = NULL;
static size_t heap_remaining = 0;

void* malloc(size_t size) {
    // Force 8-byte boundaries structurally matching standard LibC parameters!
    if (size == 0) return NULL;
    if (size % 8 != 0) {
        size += (8 - (size % 8));
    }
    
    // If the active physical page frame doesn't hold enough memory blocks, ping PMM!
    if (heap_cursor == NULL || heap_remaining < size) {
        // Request a brand new pristine 4KB chunk organically strictly from the physical motherboard!
        heap_cursor = (uint8_t*)pmm_alloc_page();
        if(!heap_cursor) return NULL;
        heap_remaining = 4096;
    }
    
    void* allocated_memory = heap_cursor;
    heap_cursor += size;
    heap_remaining -= size;
    
    return allocated_memory;
}

void free(void* ptr) {
    (void)ptr; 
    // Disclaimer: An ultra-basic sequential bump-allocator fundamentally cannot actually 'free' memory internally! 
    // True free() logic strictly requires deploying complicated Header Checksums identically tracking list bounds. 
    // It remains pseudo-mapped but strictly harmless for basic structural tests natively.
}
