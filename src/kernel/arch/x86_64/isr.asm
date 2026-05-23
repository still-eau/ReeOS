; Interrupt Service Routine (ISR) stubs for x86_64.
; Each stub preserves all general-purpose registers, passes the saved frame
; to the matching C handler, then restores state and returns with iretq.
; Two variants exist: exceptions that push no error code (dummy 0 is pushed
; to keep the frame uniform) and those where the CPU pushes one automatically.

[bits 64]

; exported to idt.c as extern symbols
global isr_stub_default
global isr_divide_error
global isr_invalid_opcode
global isr_double_fault
global isr_general_protection
global isr_page_fault

; C handlers declared in interrupts.c
extern c_isr_default
extern c_isr_divide_error
extern c_isr_invalid_opcode
extern c_isr_double_fault
extern c_isr_general_protection
extern c_isr_page_fault

; ---------------------------------------------------------------------------
; isr_no_error_code <label> <c_handler>
;   For exceptions that do NOT push an error code (vectors 0, 6, and default).
;   A dummy 0 is pushed so the frame layout stays identical to the error-code
;   variant: [r15..rax | dummy_err | rip | cs | rflags | rsp | ss]
; ---------------------------------------------------------------------------
%macro isr_no_error_code 2
%1:
    push    qword 0             ; dummy error code to normalise the frame
    push    rax                 ; save caller-saved and callee-saved GPRs
    push    rbx
    push    rcx
    push    rdx
    push    rsi
    push    rdi
    push    rbp
    push    r8
    push    r9
    push    r10
    push    r11
    push    r12
    push    r13
    push    r14
    push    r15
    mov     rdi, rsp            ; 1st arg (System V ABI): pointer to saved frame
    call    %2                  ; call the C handler
    pop     r15                 ; restore all GPRs in reverse order
    pop     r14
    pop     r13
    pop     r12
    pop     r11
    pop     r10
    pop     r9
    pop     r8
    pop     rbp
    pop     rdi
    pop     rsi
    pop     rdx
    pop     rcx
    pop     rbx
    pop     rax
    add     rsp, 8              ; discard the dummy error code
    iretq                       ; return from interrupt (restores rip/cs/rflags/rsp/ss)
%endmacro

; ---------------------------------------------------------------------------
; isr_with_error_code <label> <c_handler>
;   For exceptions where the CPU pushes an error code before transferring
;   control (vectors 8, 13, 14). The code is already on the stack on entry.
; ---------------------------------------------------------------------------
%macro isr_with_error_code 2
%1:
    ; CPU has already pushed: ss, rsp, rflags, cs, rip, error_code
    push    rax                 ; save all GPRs on top of the CPU-pushed error code
    push    rbx
    push    rcx
    push    rdx
    push    rsi
    push    rdi
    push    rbp
    push    r8
    push    r9
    push    r10
    push    r11
    push    r12
    push    r13
    push    r14
    push    r15
    mov     rdi, rsp            ; 1st arg (System V ABI): pointer to saved frame
    call    %2                  ; call the C handler
    pop     r15                 ; restore all GPRs in reverse order
    pop     r14
    pop     r13
    pop     r12
    pop     r11
    pop     r10
    pop     r9
    pop     r8
    pop     rbp
    pop     rdi
    pop     rsi
    pop     rdx
    pop     rcx
    pop     rbx
    pop     rax
    add     rsp, 8              ; discard the CPU-pushed error code
    iretq                       ; return from interrupt
%endmacro

; ---------------------------------------------------------------------------
; ISR stubs - one entry point per registered exception vector
; ---------------------------------------------------------------------------

; default fallback for all 256 unregistered IDT vectors (no error code)
isr_no_error_code   isr_stub_default,       c_isr_default

; vector 0  - #DE divide-by-zero, triggered by DIV/IDIV with zero divisor
isr_no_error_code   isr_divide_error,       c_isr_divide_error

; vector 6  - #UD invalid or undefined opcode in the instruction stream
isr_no_error_code   isr_invalid_opcode,     c_isr_invalid_opcode

; vector 8  - #DF double fault, CPU pushes error code 0, routed via IST1
isr_with_error_code isr_double_fault,       c_isr_double_fault

; vector 13 - #GP general protection fault, CPU pushes a segment error code
isr_with_error_code isr_general_protection, c_isr_general_protection

; vector 14 - #PF page fault, CPU pushes error code, CR2 = faulting address
isr_with_error_code isr_page_fault,         c_isr_page_fault
