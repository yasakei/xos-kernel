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

#ifndef PMM_H
#define PMM_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    size_t total_pages;     // total pages in address space
    size_t reserved_pages;  // pages reserved for kernel/hardware (0-2mb)
    size_t used_pages;      // allocated pages in the free pool
    size_t free_pages;      // available pages in the free pool
    size_t page_size;       // bytes per page
} pmm_stats_t;

void  pmm_init(void);
void* pmm_alloc_page(void);
void  pmm_free_page(void* ptr);
void  pmm_get_stats(pmm_stats_t *stats);

#endif
