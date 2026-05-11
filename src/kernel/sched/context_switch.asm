[BITS 64]

; context_switch(task_context_t *old_ctx, task_context_t *new_ctx)
;
; rdi = pointer to old task's task_context_t  (save here)
; rsi = pointer to new task's task_context_t  (load from here)
;
; Layout of task_context_t (must match scheduler.h):
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
    ; ── Save old task ────────────────────────────────────────────────────────
    mov [rdi +  0], r15
    mov [rdi +  8], r14
    mov [rdi + 16], r13
    mov [rdi + 24], r12
    mov [rdi + 32], rbx
    mov [rdi + 40], rbp
    ; Save rsp (points to return address right now, after the call pushed it)
    mov [rdi + 48], rsp
    ; Save rip = the return address sitting at [rsp]
    mov rax, [rsp]
    mov [rdi + 56], rax

    ; ── Load new task ────────────────────────────────────────────────────────
    mov r15, [rsi +  0]
    mov r14, [rsi +  8]
    mov r13, [rsi + 16]
    mov r12, [rsi + 24]
    mov rbx, [rsi + 32]
    mov rbp, [rsi + 40]
    ; Switch to the new task's stack
    mov rsp, [rsi + 48]
    ; Put the new task's rip on top of the new stack, then ret into it
    mov rax, [rsi + 56]
    mov [rsp], rax

    ret
