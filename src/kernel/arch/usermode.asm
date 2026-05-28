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

; drops from ring 0 to ring 3 using iretq.
; the iretq instruction pops rip, cs, rflags, rsp, and ss from the stack,
; so we just push the user-mode values and fire it off.

[BITS 64]

; usermode_enter(uint64_t user_entry, uint64_t user_stack)
; rdi = user entry point (rip)
; rsi = user stack pointer (rsp)
;
; user selectors (rpl=3):
;   user code:  0x1b  (gdt index 3, rpl 3)
;   user data:  0x23  (gdt index 4, rpl 3)

global usermode_enter
usermode_enter:
    ; build the iretq frame on the kernel stack
    ; order on stack (pushed last-first): ss, rsp, rflags, cs, rip
    
    push qword 0x23        ; ss — user data segment with ring 3
    push rsi               ; rsp — user-mode stack pointer
    push qword 0x202       ; rflags — interrupts enabled (if=1), bit 1 always 1
    push qword 0x1B        ; cs — user code segment with ring 3
    push rdi               ; rip — where user code starts
    
    iretq                  ; pop everything and land in user mode!
    
    ; we should never end up here, but just in case, scream 'a' and halt
    mov al, 'A'
    mov dx, 0x3F8
    out dx, al
    hlt
