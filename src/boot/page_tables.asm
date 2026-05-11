[BITS 64]

; Page tables for 64-bit mode
; Located at 0x1000 and beyond

; PML4 table at 0x1000
align 0x1000
pml4:
    dq 0x2000 + 0x03    ; Point to PDPT at 0x2000
    times 511 dq 0

; PDPT at 0x2000
align 0x1000
pdpt:
    dq 0x3000 + 0x03    ; Point to PD at 0x3000
    times 511 dq 0

; PD at 0x3000 with 2MB pages
align 0x1000
pd:
    dq 0x00000000 + 0x83    ; 0-2MB
    dq 0x00200000 + 0x83    ; 2-4MB
    dq 0x00400000 + 0x83    ; 4-6MB
    dq 0x00600000 + 0x83    ; 6-8MB
    dq 0x00800000 + 0x83    ; 8-10MB
    dq 0x00A00000 + 0x83    ; 10-12MB
    dq 0x00C00000 + 0x83    ; 12-14MB
    dq 0x00E00000 + 0x83    ; 14-16MB
    times 504 dq 0
