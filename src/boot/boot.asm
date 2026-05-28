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

; hey, this is the master boot record for xos.
; it lives at 0x7c00, runs in 16-bit real mode, loads the rest of the kernel
; from disk using bios int 13h extended reads, then jumps to protected mode.

[BITS 16]
[ORG 0x7C00]

jmp start

; ── data area ───────────────────────────────────────────────────────────
; these variables get stuffed in the code region to save space.
lba_sector dw 1       ; start at lba sector 1
sectors_to_read db 255 ; read 255 sectors (~127 kb)
load_segment dw 0x0800 ; load at 0x0800:0000 = physical 0x8000
boot_drive db 0

; disk address packet used by int 13h extended reads
align 4
dap:
    db 0x10            ; packet size (16 bytes)
    db 0x00            ; reserved
    dw 1               ; sectors per transfer
    dw 0x0000          ; offset
    dw 0x0800          ; segment
    dq 0x0000000000000001

; ── serial port i/o ─────────────────────────────────────────────────────
; set up the com1 serial port so we can print debug chars.
serial_init:
    mov dx, 0x3FB       ; line control register
    mov al, 0x80        ; enable dlab so we can set baud rate
    out dx, al
    
    mov dx, 0x3F8       ; divisor low byte (3 = 38400 baud)
    mov al, 3
    out dx, al
    
    mov dx, 0x3F9       ; divisor high byte
    mov al, 0
    out dx, al
    
    mov dx, 0x3FB       ; disable dlab, set 8 bits, 1 stop, no parity
    mov al, 0x03
    out dx, al
    
    ret

; send a single character over serial. the char goes in cl.
serial_putchar:
    mov dx, 0x3FD       ; line status register
.wait_ready:
    in al, dx
    test al, 0x20       ; is the transmit buffer empty?
    jz .wait_ready      ; nope, keep waiting
    
    mov al, cl          ; grab the character
    mov dx, 0x3F8       ; data register
    out dx, al
    ret

; ── entry point ─────────────────────────────────────────────────────────
start:
    cli                 ; no interrupts yet, we're setting up
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00      ; stack grows down from just below us
    mov [boot_drive], dl ; bios stored boot drive in dl, save it
    
    ; initialize com1 serial port all over again (belt and suspenders)
    mov dx, 0x3FB       ; line control register  
    mov al, 0x80        ; set dlab
    out dx, al
    
    mov dx, 0x3F8       ; divisor low
    mov al, 0x03        ; 38400 baud
    out dx, al
    
    mov dx, 0x3F9       ; divisor high
    xor al, al
    out dx, al
    
    mov dx, 0x3FB       ; line control
    mov al, 0x03        ; 8n1
    out dx, al
    
    ; send 's' over serial so we know boot started
    mov dx, 0x3FD       ; line status
.wait_s:
    in al, dx
    test al, 0x20
    jz .wait_s
    mov dx, 0x3F8       ; data port
    mov al, 'S'
    out dx, al

    ; ── disk load loop ──────────────────────────────────────────────────
    ; read the kernel from disk sector by sector using bios extended read.
.load_loop:
    cmp byte [sectors_to_read], 0
    je .done_loading     ; all done? jump ahead

    ; set up the dap with the current lba and load segment
    xor eax, eax
    mov ax, [lba_sector]
    mov dword [dap + 8], eax
    xor eax, eax
    mov dword [dap + 12], eax
    mov ax, [load_segment]
    mov word [dap + 4], 0x0000
    mov [dap + 6], ax

    mov si, dap
    mov dl, [boot_drive]
    mov ah, 0x42
    int 0x13
    jnc .read_ok        ; carry clear = it worked
    
    ; read failed, send 'e' to serial and bail
    mov dx, 0x3FD
.wait_e:
    in al, dx
    test al, 0x20
    jz .wait_e
    mov dx, 0x3F8
    mov al, 'E'
    out dx, al
    jmp disk_error
    
.read_ok:
    ; on to the next sector
    inc word [lba_sector]
    dec byte [sectors_to_read]
    
    ; advance the load segment by 32 paragraphs (512 bytes)
    mov ax, [load_segment]
    add ax, 0x0020
    mov [load_segment], ax
    
    jmp .load_loop

.done_loading:
    ; send 'd' to serial so we know loading is done
    mov dx, 0x3FD
.wait_d:
    in al, dx
    test al, 0x20
    jz .wait_d
    mov dx, 0x3F8
    mov al, 'D'
    out dx, al
    
    ; also print 'd' to vga text mode
    mov ah, 0x0E
    mov al, 'D'
    xor bx, bx
    int 0x10
    
    ; load the gdt so we can switch to protected mode
    lgdt [gdt_desc]

; ── enter protected mode ────────────────────────────────────────────────
; ok let's flip the big switch. just set the pe bit in cr0.
mov eax, cr0
or eax, 1
mov cr0, eax

; far jump to reload cs with our 32-bit code selector
jmp 0x08:protected_mode

; ── disk error handler ──────────────────────────────────────────────────
; if disk reads fail we hang here and scream 'err' at you.
disk_error:
    mov dx, 0x3FD
.wait_err:
    in al, dx
    test al, 0x20
    jz .wait_err
    mov dx, 0x3F8
    mov al, 'E'
    out dx, al
    
    mov ah, 0x0E
    mov al, 'E'
    xor bx, bx
    int 0x10
    mov al, 'R'
    int 0x10
    mov al, 'R'
    int 0x10
    
.halt:
    hlt
    jmp .halt

; ── global descriptor table ─────────────────────────────────────────────
; minimal gdt: null, 32-bit code, 32-bit data.
align 8
gdt:
    dq 0                            ; null descriptor
    dq 0x00cf9a000000ffff           ; code descriptor
    dq 0x00cf92000000ffff           ; data descriptor
gdt_end:

gdt_desc:
    dw gdt_end - gdt - 1            ; gdt size - 1
    dd gdt                          ; gdt linear address

[BITS 32]
; ── protected mode entry ────────────────────────────────────────────────
; we're now in 32-bit protected mode. set up segments and jump to kernel.
protected_mode:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax
    
    mov esp, 0x7000                 ; give ourselves a stack
    
    jmp 0x08:0x8000                 ; hand off to the kernel at 0x8000

; pad to 510 bytes (boot sector must be 512 bytes total)
times 510 - ($ - $$) db 0

; boot signature — bios checks this magic number
dw 0xAA55
