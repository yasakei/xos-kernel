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

; saves the current task's registers and loads the next task's registers.
; this is the heart of the scheduler's context switch.

[BITS 64]

; context_switch(task_context_t *old_ctx, task_context_t *new_ctx)
; rdi = pointer to old task's context (we save current regs here)
; rsi = pointer to new task's context (we load from here)
;
; layout of task_context_t (must match scheduler.h):
;   +0:  r15
;   +8:  r14
;   +16: r13
;   +24: r12
;   +32: rbx
;   +40: rbp
;   +48: rsp
;   +56: rip

global context_switch
context_switch:
    ; ── save the currently running task's registers ────────────────────
    mov [rdi +  0], r15
    mov [rdi +  8], r14
    mov [rdi + 16], r13
    mov [rdi + 24], r12
    mov [rdi + 32], rbx
    mov [rdi + 40], rbp
    ; save the stack pointer — right now rsp points at the return address
    mov [rdi + 48], rsp
    ; save the return address as the instruction pointer
    mov rax, [rsp]
    mov [rdi + 56], rax

    ; ── load the new task's registers ──────────────────────────────────
    mov r15, [rsi +  0]
    mov r14, [rsi +  8]
    mov r13, [rsi + 16]
    mov r12, [rsi + 24]
    mov rbx, [rsi + 32]
    mov rbp, [rsi + 40]
    ; switch to the new task's stack
    mov rsp, [rsi + 48]
    ; put the new rip on top of the new stack, then ret into it
    mov rax, [rsi + 56]
    mov [rsp], rax

    ret
