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

; this is the first thing that runs after the bootloader jumps to 0x8000.
; it sets up long mode page tables, enables pae + paging, loads a 64-bit
; gdt, and finally calls kernel_main.

[BITS 32]

global kernel_entry
extern kernel_main

section .text
kernel_entry:
    ; set up a temporary stack — we'll move it later in 64-bit mode
    mov esp, 0x7000
    
    ; step 1: clear the page table region (0x1000 - 0x3fff)
    mov edi, 0x1000
    mov cr3, edi
    xor eax, eax
    mov ecx, 3072        ; 3072 dwords = 12 kb of zeroes
    rep stosd

    ; step 2: set up pml4 entry at 0x1000 pointing to pdpt at 0x2000
    mov dword [0x1000], 0x2007

    ; step 3: set up pdpt entry at 0x2000 pointing to pd at 0x3000
    mov dword [0x2000], 0x3007

    ; step 4: build the page directory at 0x3000 with 128 2mb huge pages
    ; this maps the first 256 mb of physical memory.
    mov edi, 0x3000
    mov eax, 0x00000087      ; base 0 + present + rw + user + huge page
    mov ecx, 128             ; 128 pages * 2mb = 256 mb total
.build_vmm:
    mov [edi], eax
    add eax, 0x200000        ; next 2 mb chunk
    add edi, 8               ; next pde slot
    loop .build_vmm

    ; step 5: enable pae (physical address extension) in cr4
    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax

    ; step 6: point cr3 at our pml4 table
    mov eax, 0x1000
    mov cr3, eax

    ; step 7: enable long mode by setting efer.lme
    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    ; step 8: enable paging by setting cr0.pg
    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax

    ; step 9: load the 64-bit gdt
    ; we use an explicit 32-bit absolute pointer register here to avoid
    ; any mode-dependent memory operand ambiguity at the ia-32e transition.
    mov eax, gdt64.pointer
    lgdt [eax]

    ; step 10: far jump into 64-bit mode!
    jmp 0x08:long_mode

[BITS 64]
long_mode:
    ; reset all segment registers with the 64-bit data selector
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; move the stack way up high so it doesn't collide with the kernel
    ; binary (kernel loads at 0x8000, stack at 0x1f0000 gives ~1.9 mb room)
    mov rsp, 0x1F0000

    ; send 'k' over serial to prove we're alive in 64-bit land
    mov al, 'K'
    mov dx, 0x3F8
    out dx, al

    ; finally, call the main kernel code
    call kernel_main

    ; we shouldn't return, but just in case, halt
    cli
.halt:
    hlt
    jmp .halt

; ── 64-bit gdt ──────────────────────────────────────────────────────────
section .data
align 8
gdt64:
    dq 0 ; null descriptor
.code: equ $ - gdt64
    dq 0x00209A0000000000 ; 64-bit code segment
.data: equ $ - gdt64
    dq 0x0000920000000000 ; 64-bit data segment
.pointer:
    dw $ - gdt64 - 1      ; limit
    dq gdt64              ; base address
