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

; user-mode "hello world" process.
; prints a message via syscall and then yields forever.

[BITS 64]

global _start

section .text
_start:
    ; syscall 0 = write — prints a string to the debug console
    mov rax, 0
    lea rdi, [rel msg]
    int 0x80

.loop:
    ; syscall 1 = yield — give the cpu back to the scheduler
    mov rax, 1
    int 0x80
    jmp .loop

section .rodata
msg: db "Hello from HELLO.ELF!", 10, 0
