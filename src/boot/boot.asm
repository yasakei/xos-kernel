[BITS 16]
[ORG 0x7C00]

jmp start

; Data area
lba_sector dw 1       ; Start at LBA sector 1
sectors_to_read db 50 ; Read 50 sectors (25 KB)
load_segment dw 0x0800 ; Load at 0x0800:0000 = physical 0x8000

; Serial port I/O
serial_init:
    mov dx, 0x3FB       ; Line control register
    mov al, 0x80        ; Enable DLAB
    out dx, al
    
    mov dx, 0x3F8       ; Divisor low (set to 3 for 38400)
    mov al, 3
    out dx, al
    
    mov dx, 0x3F9       ; Divisor high
    mov al, 0
    out dx, al
    
    mov dx, 0x3FB       ; Disable DLAB, 8 bits, 1 stop, no parity
    mov al, 0x03
    out dx, al
    
    ret

serial_putchar:
    ; AL = character to send
    mov dx, 0x3FD       ; Line status register
.wait_ready:
    in al, dx
    test al, 0x20       ; Check transmit empty bit
    jz .wait_ready
    
    mov al, cl          ; Get character back
    mov dx, 0x3F8       ; Data register
    out dx, al
    ret

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    
    ; Initialize serial (set up COM1 port)
    mov dx, 0x3FB       ; Line control register  
    mov al, 0x80        ; Set DLAB
    out dx, al
    
    mov dx, 0x3F8       ; Divisor low
    mov al, 0x03        ; 38400 baud
    out dx, al
    
    mov dx, 0x3F9       ; Divisor high
    xor al, al
    out dx, al
    
    mov dx, 0x3FB       ; Line control
    mov al, 0x03        ; 8N1
    out dx, al
    
    ; Send 'S' to serial
    mov dx, 0x3FD       ; Line status
.wait_s:
    in al, dx
    test al, 0x20
    jz .wait_s
    mov dx, 0x3F8       ; Data port
    mov al, 'S'
    out dx, al

.load_loop:
    cmp byte [sectors_to_read], 0
    je .done_loading

    ; Send 'R' to serial
    mov dx, 0x3FD
.wait_r:
    in al, dx
    test al, 0x20
    jz .wait_r
    mov dx, 0x3F8
    mov al, 'R'
    out dx, al
    
    ; Convert LBA to CHS
    mov ax, [lba_sector]
    xor dx, dx
    mov cx, 18
    div cx              ; AX = LBA / 18, DX = LBA % 18
    mov cl, dl
    inc cl              ; CL = (LBA % 18) + 1 = sector
    
    mov dx, ax         ; DX = LBA / 18
    xor ax, ax
    mov al, dl         ; AL = LBA / 18
    mov dl, 2
    div dl             ; AL = (LBA/18) / 2 = cylinder, AH = (LBA/18) % 2 = head
    
    mov ch, al         ; CH = cylinder
    mov dh, ah         ; DH = head
    mov dl, 0x00       ; DL = drive (floppy 0)
    
    ; Set up registers for read
    mov bx, [load_segment]
    mov es, bx
    xor bx, bx         ; ES:BX = buffer address
    mov ax, 0x0201     ; AH = 0x02 (read), AL = 1 (sector count)
    
    int 0x13
    jnc .read_ok
    
    ; Send 'E' to serial
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
    ; Send 'G' to serial
    mov dx, 0x3FD
.wait_g:
    in al, dx
    test al, 0x20
    jz .wait_g
    mov dx, 0x3F8
    mov al, 'G'
    out dx, al

    ; Increment counters
    inc word [lba_sector]
    dec byte [sectors_to_read]
    
    ; Move to next segment
    mov ax, [load_segment]
    add ax, 0x0020      ; Add 32 paragraphs (512 bytes)
    mov [load_segment], ax
    
    jmp .load_loop

.done_loading:
    ; Send final 'D' to serial before entering protected mode
    mov dx, 0x3FD
.wait_d:
    in al, dx
    test al, 0x20
    jz .wait_d
    mov dx, 0x3F8
    mov al, 'D'
    out dx, al
    
    ; Also print to VGA
    mov ah, 0x0E
    mov al, 'D'
    xor bx, bx
    int 0x10
    
    ; Setup GDT
lgdt [gdt_desc]

; Enter protected mode
mov eax, cr0
or eax, 1
mov cr0, eax

; Jump to protected mode code
jmp 0x08:protected_mode

disk_error:
    ; Send 'E' to serial AND VGA to ensure visibility
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

; Global Descriptor Table
align 8
gdt:
    dq 0                            ; Null descriptor
    dq 0x00cf9a000000ffff           ; Code descriptor
    dq 0x00cf92000000ffff           ; Data descriptor
gdt_end:

gdt_desc:
    dw gdt_end - gdt - 1            ; GDT size
    dd gdt                          ; GDT address

[BITS 32]
protected_mode:
    ; Set up segment registers
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax
    
    ; Set up stack
    mov esp, 0x7000
    
    ; Jump to kernel
    jmp 0x08:0x8000

; Padding to align to 512 bytes (except boot signature)
times 510 - ($ - $$) db 0

; Boot signature
dw 0xAA55
