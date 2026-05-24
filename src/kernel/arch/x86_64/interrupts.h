// ReeOS - Kernel Architecture
#pragma once
#include <stdint.h>

typedef struct __attribute__((packed)) {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t vec;
    uint64_t err;
    uint64_t rip, cs, rflags, rsp, ss;
} ISRFrame;

// Dynamic IRQ handler registration (0–15)
typedef void (*irq_callback_t)(uint8_t irq, ISRFrame *frame);
void irq_register_handler(uint8_t irq, irq_callback_t cb);

// Exceptions
void c_isr_default            (ISRFrame *f);
void c_isr_divide_error       (ISRFrame *f);
void c_isr_debug              (ISRFrame *f);
void c_isr_nmi                (ISRFrame *f);
void c_isr_breakpoint         (ISRFrame *f);
void c_isr_overflow           (ISRFrame *f);
void c_isr_bound_range        (ISRFrame *f);
void c_isr_invalid_opcode     (ISRFrame *f);
void c_isr_device_not_avail   (ISRFrame *f);
__attribute__((noreturn)) void c_isr_double_fault(ISRFrame *f);
void c_isr_invalid_tss        (ISRFrame *f);
void c_isr_segment_not_present(ISRFrame *f);
void c_isr_stack_fault        (ISRFrame *f);
void c_isr_general_protection (ISRFrame *f);
void c_isr_page_fault         (ISRFrame *f);
void c_isr_fpu_error          (ISRFrame *f);
void c_isr_alignment_check    (ISRFrame *f);
__attribute__((noreturn)) void c_isr_machine_check(ISRFrame *f);
void c_isr_simd_error         (ISRFrame *f);

// IRQs and syscall
void c_irq_handler    (ISRFrame *f, uint64_t irq_num);
void c_syscall_handler(ISRFrame *f);