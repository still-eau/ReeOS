; =============================================================================
;  ReeOS - Kernel Entry Point
; =============================================================================
;
;  This file is the very first code that executes after the bootloader hands
;  control to the kernel.
;
;  Boot contract (enforced by boot2.asm):
;   - CPU is in 64-bit long mode with a flat GDT (CS=0x08, DS=0x00)
;   - 4-level identity-mapping covers the first 1 GB of physical RAM
;   - The kernel image starts at physical address 0x100000 (this file = byte 0)
;   - Interrupts are disabled (cli was issued before the far jump)
;   - Segment registers ds/es/fs/gs/ss are zeroed
;   - rsp still points at the bootloader's temporary stack (0x90000) - we
;     replace it immediately with the kernel's own stack defined below
;
;  Execution flow:
;    _start  ->  gdt_init  ->  idt_init  ->  kernel_main  ->  .halt_loop
;
; =============================================================================

[bits 64]
[global _start]

; C functions provided by the rest of the kernel
extern kernel_main

; Kernel stack
; 16 KB, 16-byte aligned (System V AMD64 ABI requires 16-byte stack
; alignment before every CALL instruction).
section .bss
align 16
kernel_stack_bottom:
    resb 16384              ; 16 KB of stack space
global kernel_stack_top
kernel_stack_top:           ; stack grows downward, so RSP starts here

; Read-only data
section .rodata
msg_kernel_panic: db "PANIC: kernel_main returned", 0

; Kernel entry - must be the first code in the .text section so the linker
; places it at the very beginning of the output binary (= 0x100000).
section .text

_start:
    ; 1. Install a proper kernel stack
    ; The bootloader left RSP at 0x90000 (inside the BSS area we may
    ; later overwrite).  Switch to the kernel's own statically allocated
    ; stack before doing anything that touches the stack.
    lea     rsp, [rel kernel_stack_top]

    ; Align RSP to 16 bytes as mandated by the System V AMD64 ABI.
    ; The 'and' clears the low 4 bits; since kernel_stack_top is already
    ; 16-byte aligned (align 16 directive above), this is a safety measure.
    and     rsp, ~0xF

    ; 2. Clear the base pointer
    xor     rbp, rbp

    ; 3. Jump directly into the C kernel
    ; The kernel module orchestrator in kernel_main will reload the GDT,
    ; install the IDT, configure the PIC, and enable interrupts as modules.
    call    kernel_main

    ; 7. Halt if kernel_main ever returns (should never happen)
    ; Print a minimal panic message to the VGA text buffer so the operator
    ; knows what went wrong, then spin forever.
.halt_loop:
    cli
    hlt
    jmp     .halt_loop
