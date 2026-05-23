#ifndef INTERRUPTS_H
#define INTERRUPTS_H

// Shared types and declarations for CPU exception handlers.
// InterruptFrame matches exactly what isr.asm pushes before calling any c_isr_* function.

#include <stdint.h>

// Saved CPU state as laid out on the stack by the ISR stubs.
// The CPU-pushed tail (rip, cs, rflags, rsp, ss) is appended automatically on interrupt entry.
typedef struct __attribute__((packed))
{
    uint64_t r15, r14, r13, r12;
    uint64_t r11, r10, r9,  r8;
    uint64_t rbp, rdi, rsi, rdx;
    uint64_t rcx, rbx, rax;
    uint64_t error_code;    // 0 for no-error-code exceptions, CPU value for the rest
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;

} InterruptFrame;

void c_isr_default(InterruptFrame *frame);
void c_isr_divide_error(InterruptFrame *frame);
void c_isr_invalid_opcode(InterruptFrame *frame);
void c_isr_double_fault(InterruptFrame *frame);
void c_isr_general_protection(InterruptFrame *frame);
void c_isr_page_fault(InterruptFrame *frame);

#endif
