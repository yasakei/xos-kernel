#ifndef PMM_H
#define PMM_H

#include <stdint.h>
#include <stddef.h>

void pmm_init(void);
void* pmm_alloc_page(void);
void pmm_free_page(void* ptr);

#endif
