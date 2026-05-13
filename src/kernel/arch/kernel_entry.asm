[BITS 32]

global kernel_entry
extern kernel_main

section .text
kernel_entry:
    ; Set up stack
    mov esp, 0x7000
    
    ; 1. Clear page tables at 0x1000 - 0x3FFF
    mov edi, 0x1000
    mov cr3, edi
    xor eax, eax
    mov ecx, 3072
    rep stosd

    ; 2. Setup PML4 at 0x1000
    mov dword [0x1000], 0x2007

    ; 3. Setup PDPT at 0x2000
    mov dword [0x2000], 0x3007

    ; 4. Setup PD at 0x3000 to map first 256MB dynamically using 2MB huge pages!
    mov edi, 0x3000
    mov eax, 0x00000087      ; Base physical address 0 + Present + R/W + User + Huge
    mov ecx, 128             ; 128 pages * 2MB = 256MB total mapped memory natively!
.build_vmm:
    mov [edi], eax
    add eax, 0x200000        ; Increment mapped physical target by 2MB natively
    add edi, 8               ; Shift pointer to the next 64-bit page directory entry
    loop .build_vmm

    ; 5. Enable PAE (CR4.PAE = 1)
    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax

    ; 6. Set CR3
    mov eax, 0x1000
    mov cr3, eax

    ; 7. Enable long mode (EFER.LME = 1)
    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    ; 8. Enable paging (CR0.PG = 1)
    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax

    ; 9. Load 64-bit GDT
    ; Use an explicit 32-bit absolute pointer register here to avoid any
    ; mode-dependent memory operand ambiguity at the IA-32e transition point.
    mov eax, gdt64.pointer
    lgdt [eax]

    ; 10. Jump to 64-bit mode
    jmp 0x08:long_mode

[BITS 64]
long_mode:
    ; Reset segments
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Move stack to a safe region well above the kernel binary
    ; Kernel loads at 0x8000, stack at 0x1F0000 gives ~1.9MB of space
    mov rsp, 0x1F0000

    ; Write 'K' to serial port 0x3F8 to indicate we're in 64-bit mode
    mov al, 'K'
    mov dx, 0x3F8
    out dx, al

    call kernel_main

    cli
.halt:
    hlt
    jmp .halt

section .data
align 8
gdt64:
    dq 0 ; null
.code: equ $ - gdt64
    dq 0x00209A0000000000 ; 64-bit code
.data: equ $ - gdt64
    dq 0x0000920000000000 ; 64-bit data
.pointer:
    dw $ - gdt64 - 1
    dq gdt64
