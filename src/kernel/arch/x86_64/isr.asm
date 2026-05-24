; ReeOS - ISR

[bits 64]

; CPU Exceptions
global isr_stub_default
global isr_divide_error
global isr_debug
global isr_nmi
global isr_breakpoint
global isr_overflow
global isr_bound_range
global isr_invalid_opcode
global isr_device_not_avail
global isr_double_fault
global isr_invalid_tss
global isr_segment_not_present
global isr_stack_fault
global isr_general_protection
global isr_page_fault
global isr_fpu_error
global isr_alignment_check
global isr_machine_check
global isr_simd_error

; IRQs PIC
global irq_timer
global irq_keyboard
global irq_cascade
global irq_com2
global irq_com1
global irq_lpt2
global irq_floppy
global irq_lpt1
global irq_rtc
global irq_acpi
global irq_stub_a
global irq_stub_b
global irq_mouse
global irq_fpu
global irq_primary_ata
global irq_secondary_ata

; Syscall
global isr_syscall

; C handlers
extern c_isr_default
extern c_isr_divide_error
extern c_isr_debug
extern c_isr_nmi
extern c_isr_breakpoint
extern c_isr_overflow
extern c_isr_bound_range
extern c_isr_invalid_opcode
extern c_isr_device_not_avail
extern c_isr_double_fault
extern c_isr_invalid_tss
extern c_isr_segment_not_present
extern c_isr_stack_fault
extern c_isr_general_protection
extern c_isr_page_fault
extern c_isr_fpu_error
extern c_isr_alignment_check
extern c_isr_machine_check
extern c_isr_simd_error
extern c_irq_handler
extern c_syscall_handler

; PIC Constants
PIC_MASTER_CMD  equ 0x20
PIC_SLAVE_CMD   equ 0xA0
PIC_EOI         equ 0x20

; Macros

%macro save_regs 0
    push    rax
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
%endmacro

%macro restore_regs 0
    pop     r15
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
%endmacro

; isr_no_err  <label> <vec_num> <c_handler>
;   Exception without CPU error code. Pushes a 0 code + the vector number
;   so that the frame is identical to isr_with_err.
%macro isr_no_err 3
%1:
    push    qword 0     ; fake error code
    push    qword %2    ; vector number
    save_regs
    mov     rdi, rsp    ; arg1 : ISRFrame *
    call    %3
    restore_regs
    add     rsp, 16     ; retire vec + err
    iretq
%endmacro

; isr_with_err  <label> <vec_num> <c_handler>
;   Exception with CPU error code. Pushes only the vector number on top
;   to unify the frame.
%macro isr_with_err 3
%1:
    push    qword %2    ; vector number (err already there, pushed by CPU)
    save_regs
    mov     rdi, rsp
    call    %3
    restore_regs
    add     rsp, 16     ; retire vec + err
    iretq
%endmacro

; irq_master  <label> <irq_num>
;   IRQ from the master PIC (vectors 0x20–0x27). EOI sent to the master only.
;   The IRQ number is passed in rsi (2nd C arg) so that c_irq_handler
;   can dispatch without needing an additional table.
%macro irq_master 2
%1:
    push    qword 0     ; fake error code
    push    qword %2    ; IRQ number (0–7)
    save_regs
    mov     rdi, rsp    ; arg1 : ISRFrame *
    mov     rsi, %2     ; arg2 : irq_num
    call    c_irq_handler
    restore_regs
    add     rsp, 16
    mov     al, PIC_EOI
    out     PIC_MASTER_CMD, al
    iretq
%endmacro

; irq_slave  <label> <irq_num>
;   IRQ from the slave PIC (vectors 0x28–0x2F). EOI sent to the slave THEN
;   to the master (mandatory cascade chain via IRQ2).
%macro irq_slave 2
%1:
    push    qword 0
    push    qword %2    ; IRQ number (8–15)
    save_regs
    mov     rdi, rsp
    mov     rsi, %2
    call    c_irq_handler
    restore_regs
    add     rsp, 16
    mov     al, PIC_EOI
    out     PIC_SLAVE_CMD,  al  ; slave EOI first
    out     PIC_MASTER_CMD, al  ; master EOI (cascade)
    iretq
%endmacro

; CPU Exceptions Stubs

isr_no_err   isr_stub_default,        255, c_isr_default

isr_no_err   isr_divide_error,          0, c_isr_divide_error
isr_no_err   isr_debug,                 1, c_isr_debug
isr_no_err   isr_nmi,                   2, c_isr_nmi
isr_no_err   isr_breakpoint,            3, c_isr_breakpoint
isr_no_err   isr_overflow,              4, c_isr_overflow
isr_no_err   isr_bound_range,           5, c_isr_bound_range
isr_no_err   isr_invalid_opcode,        6, c_isr_invalid_opcode
isr_no_err   isr_device_not_avail,      7, c_isr_device_not_avail

; #DF — error code always 0 but pushed by CPU
isr_with_err isr_double_fault,          8, c_isr_double_fault

isr_with_err isr_invalid_tss,          10, c_isr_invalid_tss
isr_with_err isr_segment_not_present,  11, c_isr_segment_not_present
isr_with_err isr_stack_fault,          12, c_isr_stack_fault
isr_with_err isr_general_protection,   13, c_isr_general_protection
isr_with_err isr_page_fault,           14, c_isr_page_fault

isr_no_err   isr_fpu_error,            16, c_isr_fpu_error
isr_with_err isr_alignment_check,      17, c_isr_alignment_check
isr_no_err   isr_machine_check,        18, c_isr_machine_check
isr_no_err   isr_simd_error,           19, c_isr_simd_error

; PIC Master IRQs Stubs (vectors 0x20–0x27)

irq_master   irq_timer,       0
irq_master   irq_keyboard,    1
irq_master   irq_cascade,     2
irq_master   irq_com2,        3
irq_master   irq_com1,        4
irq_master   irq_lpt2,        5
irq_master   irq_floppy,      6
irq_master   irq_lpt1,        7

; PIC Slave IRQs Stubs (vectors 0x28–0x2F)

irq_slave    irq_rtc,          8
irq_slave    irq_acpi,         9
irq_slave    irq_stub_a,      10
irq_slave    irq_stub_b,      11
irq_slave    irq_mouse,       12
irq_slave    irq_fpu,         13
irq_slave    irq_primary_ata, 14
irq_slave    irq_secondary_ata, 15

; Syscall Stub INT 0x80
; From ring 3 : swapgs to access kernel GS base

isr_syscall:
    ; swapgs only if coming from ring 3 (CS & 3 == 3)
    ; Test the CS saved by CPU (at [rsp+8] after fake err push)
    push    qword 0         ; fake error code
    push    qword 0x80      ; vecteur
    save_regs

    ; Check caller DPL in saved CS
    ; Offset in frame : 15 regs × 8 + vec(8) + err(8) + rip(8) + cs(8) = 144
    mov     rax, [rsp + 144]
    and     rax, 3
    cmp     rax, 3
    jne     .kernel_call
    swapgs                  ; switch to kernel GS
.kernel_call:
    mov     rdi, rsp        ; arg1 : ISRFrame *
    call    c_syscall_handler
    ; Restore GS if coming from ring 3
    mov     rax, [rsp + 144]
    and     rax, 3
    cmp     rax, 3
    jne     .no_swapgs
    swapgs
.no_swapgs:
    restore_regs
    add     rsp, 16
    iretq