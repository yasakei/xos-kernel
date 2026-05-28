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

; simple ring-3 test program that just loops forever.
; proves we can run user-mode code without crashing.

[BITS 64]

global user_test_entry
user_test_entry:
    ; make a syscall (int 0x80) with rax=0 to print a message
    mov rax, 0          ; syscall 0 = write string
    lea rdi, [rel .msg]
    int 0x80

.loop:
    ; yield the cpu back to the kernel
    mov rax, 1          ; syscall 1 = yield
    int 0x80
    jmp .loop

.msg: db "Hello from Ring-3!", 10, 0
