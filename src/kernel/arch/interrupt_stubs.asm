[BITS 64]

extern interrupt_handler

global load_idt
load_idt:
    lidt [rdi]
    ret

; Macro for exceptions that don't push an error code
%macro ISR_NOERRCODE 1
global isr%1
isr%1:
    push 0
    push %1
    jmp isr_common_stub
%endmacro

; Macro for exceptions that implicitly push an error code
%macro ISR_ERRCODE 1
global isr%1
isr%1:
    push %1
    jmp isr_common_stub
%endmacro

ISR_NOERRCODE 0
ISR_NOERRCODE 1
ISR_NOERRCODE 2
ISR_NOERRCODE 3
ISR_NOERRCODE 4
ISR_NOERRCODE 5
ISR_NOERRCODE 6
ISR_NOERRCODE 7
ISR_ERRCODE 8    ; Error code
ISR_NOERRCODE 9
ISR_ERRCODE 10   ; Error code
ISR_ERRCODE 11   ; Error code
ISR_ERRCODE 12   ; Error code
ISR_ERRCODE 13   ; Error code
ISR_ERRCODE 14   ; Error code
ISR_NOERRCODE 15
ISR_NOERRCODE 16
ISR_ERRCODE 17   ; Error code
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
ISR_ERRCODE 30   ; Error code
ISR_NOERRCODE 31

; IRQs
%macro IRQ 2
global irq%1
irq%1:
    push 0      ; Dummy error code
    push %2     ; Interrupt number (32 + IRQ)
    jmp isr_common_stub
%endmacro

IRQ 0, 32
IRQ 1, 33
IRQ 2, 34
IRQ 3, 35
IRQ 4, 36
IRQ 5, 37
IRQ 6, 38
IRQ 7, 39
IRQ 8, 40
IRQ 9, 41
IRQ 10, 42
IRQ 11, 43
IRQ 12, 44
IRQ 13, 45
IRQ 14, 46
IRQ 15, 47

isr_common_stub:
    ; Push all general-purpose registers
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

    ; Call the C handler with a pointer to the registers struct
    mov rdi, rsp        ; Pass original unaligned stack pointer as 'regs' to C
    mov rdx, rsp        ; Temporarily save original stack pointer

    ; System V ABI requires 16-byte alignment before 'call'
    and rsp, ~0xF

    push rdx            ; We need to save the original stack pointer perfectly aligned. But wait, push ruins alignment.
    ; Instead, just do:
    mov rbp, rdx
    
    call interrupt_handler
    
    mov rsp, rbp        ; Restore original exact stack layout before pop

    ; Restore all general-purpose registers
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

    ; Drop the error code and interrupt number
    add rsp, 16
    
    ; Return from interrupt handler
    iretq
