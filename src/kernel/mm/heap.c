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

#include "heap.h"
#include "pmm.h"
#include "../lib/printf.h"
#include <stdint.h>

// a simple sequential bump-allocator kernel heap

static uint8_t* heap_cursor = NULL;
static size_t heap_remaining = 0;

// allocate memory - aligns to 8-byte boundaries, grabs new pages from pmm as needed
void* malloc(size_t size) {
    // make sure the size is aligned to 8 bytes
    if (size == 0) return NULL;
    if (size % 8 != 0) {
        size += (8 - (size % 8));
    }
    
    // grab a new page from pmm if we don't have room
    if (heap_cursor == NULL || heap_remaining < size) {
        heap_cursor = (uint8_t*)pmm_alloc_page();
        if(!heap_cursor) return NULL;
        heap_remaining = 4096;
    }
    
    void* allocated_memory = heap_cursor;
    heap_cursor += size;
    heap_remaining -= size;
    
    return allocated_memory;
}

// this is a bump allocator, so we can't actually free individual allocations
void free(void* ptr) {
    (void)ptr; 
    // a real free() would need a proper heap with linked lists and metadata
    // for now this is a no-op, which is fine for basic testing
}
