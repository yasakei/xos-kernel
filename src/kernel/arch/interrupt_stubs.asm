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

; interrupt stubs for x86-64.
; each isr/irq pushes an interrupt number (and error code if needed),
; then jumps to the common handler which saves all registers and calls
; the c interrupt_handler function.

[BITS 64]

extern interrupt_handler

; simple helper to load the idt — just wraps the lidt instruction.
global load_idt
load_idt:
    lidt [rdi]
    ret

; macro for interrupts that don't push an error code
; we push a dummy 0 to keep the stack format consistent.
%macro ISR_NOERRCODE 1
global isr%1
isr%1:
    push 0
    push %1
    jmp isr_common_stub
%endmacro

; macro for interrupts that do push an error code
; only the error code is on the stack, we add the int number.
%macro ISR_ERRCODE 1
global isr%1
isr%1:
    push %1
    jmp isr_common_stub
%endmacro

; cpu-defined exceptions 0-31
ISR_NOERRCODE 0
ISR_NOERRCODE 1
ISR_NOERRCODE 2
ISR_NOERRCODE 3
ISR_NOERRCODE 4
ISR_NOERRCODE 5
ISR_NOERRCODE 6
ISR_NOERRCODE 7
ISR_ERRCODE   8     ; double fault (has error code)
ISR_NOERRCODE 9
ISR_ERRCODE   10    ; invalid tss
ISR_ERRCODE   11    ; segment not present
ISR_ERRCODE   12    ; stack segment fault
ISR_ERRCODE   13    ; general protection fault
ISR_ERRCODE   14    ; page fault
ISR_NOERRCODE 15
ISR_NOERRCODE 16
ISR_ERRCODE   17    ; alignment check
ISR_NOERRCODE 18
ISR_NOERRCODE 19
ISR_NOERRCODE 20
ISR_NOERRCODE 21
ISR_NOERRCODE 22
ISR_NOERRCODE 23
ISR_NOERRCODE 24
ISR_NOERRCODE 25
ISR_NOERRCODE 26
ISR_NOERRCODE 27
ISR_NOERRCODE 28
ISR_NOERRCODE 29
ISR_ERRCODE   30    ; security exception
ISR_NOERRCODE 31

; macro for irq stubs — remapped pic interrupts 32-47
%macro IRQ 2
global irq%1
irq%1:
    push 0
    push %2
    jmp isr_common_stub
%endmacro

IRQ 0,  32
IRQ 1,  33
IRQ 2,  34
IRQ 3,  35
IRQ 4,  36
IRQ 5,  37
IRQ 6,  38
IRQ 7,  39
IRQ 8,  40
IRQ 9,  41
IRQ 10, 42
IRQ 11, 43
IRQ 12, 44
IRQ 13, 45
IRQ 14, 46
IRQ 15, 47

; int 0x80 — syscall gate, callable from ring 3
global isr128
isr128:
    push 0      ; dummy error code
    push 128    ; int number
    jmp isr_common_stub

; ── common interrupt handler ────────────────────────────────────────────
; this is where all isr/irq stubs converge. we save every register,
; align the stack, call the c handler, then restore everything and iretq.
isr_common_stub:
    ; save all the registers — the c handler might clobber 'em
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    ; first arg to interrupt_handler is a pointer to the saved register frame
    mov rdi, rsp

    ; align the stack to 16 bytes for the system v abi
    ; rbx already has the old rsp saved (we pushed it above)
    mov rbx, rsp
    and rsp, 0xFFFFFFFFFFFFFFF0
    sub rsp, 8              ; compensate for the return address the call will push

    call interrupt_handler

    ; put the stack pointer back so we can pop all the saved regs
    mov rsp, rbx

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    add rsp, 16     ; discard the int number and error code we pushed
    iretq
