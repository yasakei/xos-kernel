; -------------------------------------------------------------------
; mit license
; 
; copyright (c) 2026 xos
; 
; permission is hereby granted, free of charge, to any person
; obtaining a copy of this software and associated documentation
; files (the "software"), to deal in the software without
; restriction, including without limitation the rights to use,
; copy, modify, merge, publish, distribute, sublicense, and/or
; sell copies of the software, and to permit persons to whom the
; software is furnished to do so, subject to the following
; conditions:
; 
; the above copyright notice and this permission notice shall be
; included in all copies or substantial portions of the software.
; 
; the software is provided "as is", without warranty of any kind,
; express or implied, including but not limited to the warranties
; of merchantability, fitness for a particular purpose and
; noninfringement. in no event shall the authors or copyright
; holders be liable for any claim, damages or other liability,
; whether in an action of contract, tort or otherwise, arising
; from, out of or in connection with the software or the use or
; other dealings in the software.
; -------------------------------------------------------------------

; pre-built page tables for long mode.
; these sit at fixed addresses starting at 0x1000 and map the first 16 mb
; with 2 mb huge pages.

[BITS 64]

; pml4 table at 0x1000 — the top-level page map
align 0x1000
pml4:
    dq 0x2000 + 0x07    ; points to pdpt at 0x2000 (present + rw + user)
    times 511 dq 0

; pdpt at 0x2000 — page directory pointer table
align 0x1000
pdpt:
    dq 0x3000 + 0x07    ; points to pd at 0x3000 (present + rw + user)
    times 511 dq 0

; pd at 0x3000 — page directory with 2 mb huge page entries
; covers the first 16 mb of physical memory (8 entries x 2 mb each)
align 0x1000
pd:
    dq 0x00000000 + 0x87    ; 0-2 mb     (present + rw + user + huge)
    dq 0x00200000 + 0x87    ; 2-4 mb
    dq 0x00400000 + 0x87    ; 4-6 mb
    dq 0x00600000 + 0x87    ; 6-8 mb
    dq 0x00800000 + 0x87    ; 8-10 mb
    dq 0x00a00000 + 0x87    ; 10-12 mb
    dq 0x00c00000 + 0x87    ; 12-14 mb
    dq 0x00e00000 + 0x87    ; 14-16 mb
    times 504 dq 0
