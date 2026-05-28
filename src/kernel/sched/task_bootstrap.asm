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

; one-way jump into the first task's context.
; used by the scheduler to start the very first user task.

[BITS 64]

; scheduler_start_first_task(task_context_t *ctx)
; rdi = pointer to the first task's task_context_t
; this is a one-way trip — loads registers and jumps to the task entry.

global scheduler_start_first_task
scheduler_start_first_task:
    mov r15, [rdi +  0]
    mov r14, [rdi +  8]
    mov r13, [rdi + 16]
    mov r12, [rdi + 24]
    mov rbx, [rdi + 32]
    mov rbp, [rdi + 40]
    ; switch to the task's kernel stack
    mov rsp, [rdi + 48]
    ; enable interrupts and jump to the task's entry point
    sti
    jmp [rdi + 56]
