[BITS 64]

; Simple Ring-3 test program.
; Loops forever doing nothing — proves we can run in user mode without crashing.
; A syscall (int 0x80) with rax=0 prints a message via the kernel.

global user_test_entry
user_test_entry:
    ; Syscall: print "Hello from Ring-3!\n"
    mov rax, 0          ; syscall 0 = write string
    lea rdi, [rel .msg]
    int 0x80

.loop:
    ; Syscall: yield (give CPU back to kernel)
    mov rax, 1          ; syscall 1 = yield
    int 0x80
    jmp .loop

.msg: db "Hello from Ring-3!", 10, 0
