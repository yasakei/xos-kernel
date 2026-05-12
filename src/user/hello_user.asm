[BITS 64]

global _start

section .text
_start:
    ; SYS_WRITE("Hello from HELLO.ELF!\n")
    mov rax, 0
    lea rdi, [rel msg]
    int 0x80

.loop:
    ; SYS_YIELD()
    mov rax, 1
    int 0x80
    jmp .loop

section .rodata
msg: db "Hello from HELLO.ELF!", 10, 0
