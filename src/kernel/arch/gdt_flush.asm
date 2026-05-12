[BITS 64]

; gdt_flush(uint64_t gdtr_addr)
; rdi = pointer to GDTR struct
;
; We just reload GDTR and the data segments.
; CS doesn't need reloading because the new GDT has the same kernel code
; descriptor at offset 0x08 — the CPU will use it correctly on the next
; far branch or interrupt return.
global gdt_flush
gdt_flush:
    lgdt [rdi]

    ; Reload data segment registers with kernel data selector (0x10)
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ret

; tss_flush — load TSS selector 0x28 into TR
global tss_flush
tss_flush:
    mov ax, 0x28
    ltr ax
    ret
