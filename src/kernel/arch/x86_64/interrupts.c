// CPU exception handlers for x86_64.
// Each c_isr_* function is called by the matching ASM stub in isr.asm with
// a pointer to the saved InterruptFrame (rdi, System V ABI).

#include "interrupts.h"
#include <stdint.h>

// Write a panic message to VGA text buffer (white on red) and halt the CPU.
static void panic(const char *msg)
{
    volatile uint16_t *vga = (volatile uint16_t *)0xB8000;

    for (int i = 0; msg[i]; i++)
        vga[i] = 0x4F00 | (uint8_t)msg[i];

    for (;;)
        __asm__ volatile("cli; hlt");
}

// Default fallback for any unregistered IDT vector.
void c_isr_default(InterruptFrame *frame)
{
    (void)frame;
    panic("PANIC: Unhandled interrupt");
}

// Vector 0 - #DE Divide-by-Zero.
void c_isr_divide_error(InterruptFrame *frame)
{
    (void)frame;
    panic("PANIC: #DE Divide-by-Zero");
}

// Vector 6 - #UD Invalid Opcode.
void c_isr_invalid_opcode(InterruptFrame *frame)
{
    (void)frame;
    panic("PANIC: #UD Invalid Opcode");
}

// Vector 8 - #DF Double Fault. error_code is always 0.
void c_isr_double_fault(InterruptFrame *frame)
{
    (void)frame;
    panic("PANIC: #DF Double Fault");
}

// Vector 13 - #GP General Protection Fault. error_code encodes the segment selector.
void c_isr_general_protection(InterruptFrame *frame)
{
    (void)frame;
    panic("PANIC: #GP General Protection Fault");
}

// Vector 14 - #PF Page Fault. CR2 holds the faulting virtual address.
void c_isr_page_fault(InterruptFrame *frame)
{
    uint64_t fault_addr;
    __asm__ volatile("mov %%cr2, %0" : "=r"(fault_addr));

    (void)frame;
    (void)fault_addr;
    panic("PANIC: #PF Page Fault");
}
