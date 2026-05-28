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

#ifndef GDT_H
#define GDT_H

#include <stdint.h>

// segment selectors
#define GDT_KERNEL_CODE  0x08
#define GDT_KERNEL_DATA  0x10
#define GDT_USER_CODE    0x1B   // 0x18 | rpl=3
#define GDT_USER_DATA    0x23   // 0x20 | rpl=3
#define GDT_TSS          0x28

// initialize gdt with user segments + tss, load it
void gdt_init(void);

// update tss.rsp0 (kernel stack pointer for ring-3 -> ring-0 transitions)
void tss_set_kernel_stack(uint64_t rsp0);

#endif
