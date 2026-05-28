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

; helper functions to reload the gdt and install the tss.

[BITS 64]

; gdt_flush(uint64_t gdtr_addr)
; rdi = pointer to a gdtr struct (limit + base)
; we reload the gdt and the data segment registers.
; cs doesn't need reloading because the new gdt has the same kernel code
; descriptor at offset 0x08 — the cpu will use it correctly later.
global gdt_flush
gdt_flush:
    lgdt [rdi]

    ; reload the data segments with the kernel data selector (0x10)
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ret

; tss_flush — load the tss selector 0x28 into the task register
global tss_flush
tss_flush:
    mov ax, 0x28
    ltr ax
    ret
