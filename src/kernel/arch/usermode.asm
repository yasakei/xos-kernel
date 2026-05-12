[BITS 64]

; usermode_enter(uint64_t user_entry, uint64_t user_stack)
; rdi = user entry point
; rsi = user stack pointer
;
; Uses iretq to drop from Ring-0 to Ring-3.
; iretq pops: RIP, CS, RFLAGS, RSP, SS
;
; User selectors (RPL=3):
;   User code:  0x1B  (GDT index 3, RPL 3)
;   User data:  0x23  (GDT index 4, RPL 3)

global usermode_enter
usermode_enter:
    ; rdi = user entry point (RIP)
    ; rsi = user stack pointer (RSP)
    
    ; Push iretq frame
    ; Order: RIP, CS, RFLAGS, RSP, SS (popped in reverse)
    push qword 0x23        ; SS (user data selector)
    push rsi               ; RSP (user stack top)
    push qword 0x202       ; RFLAGS (IF=1, bit 1=1)
    push qword 0x1B        ; CS (user code selector)
    push rdi               ; RIP (user entry)
    
    iretq
    
    ; Should never reach here
    mov al, 'A'
    mov dx, 0x3F8
    out dx, al
    hlt
