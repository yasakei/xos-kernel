[BITS 64]

; scheduler_start_first_task(task_context_t *ctx)
;
; rdi = pointer to first task's task_context_t
;
; One-way jump — loads the task's stack and jumps to its entry point.

global scheduler_start_first_task
scheduler_start_first_task:
    mov r15, [rdi +  0]
    mov r14, [rdi +  8]
    mov r13, [rdi + 16]
    mov r12, [rdi + 24]
    mov rbx, [rdi + 32]
    mov rbp, [rdi + 40]
    ; Switch to the task's stack
    mov rsp, [rdi + 48]
    ; Jump to entry point
    jmp [rdi + 56]
