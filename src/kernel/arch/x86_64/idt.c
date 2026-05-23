/*
 * ============================================================================
 *  ReeOS - Interrupt Descriptor Table (IDT) Implementation
 * ============================================================================
 */

#include "idt.h"
#include "gdt.h"
#include <stdint.h>

// IDT table static
static IDTGate idt_table[IDT_SIZE];

static void idt_flush(IDTR *idtr) {
    asm volatile("lidt %0" : : "m"(*idtr) : "memory");
}

// idt set gate function, sets a gate in the IDT table
void idt_set_gate(uint8_t index, uint64_t handler,
                    uint16_t selector, uint8_t flags, uint8_t ist) 
{
    idt_table[index].offset_low = (uint16_t)(handler);
    idt_table[index].offset_mid = (uint16_t)(handler >> 16);
    idt_table[index].offset_high = (uint32_t)(handler >> 32);
    idt_table[index].selector = selector;
    idt_table[index].ist = ist & 0x07;
    idt_table[index].type_attributes = flags;
    idt_table[index].reserved = 0;
}

void idt_set_handler(uint8_t index, uint64_t handler)
{
    idt_set_gate(index, handler, GDT_KERNEL_CODE_SEL,
                    IDT_PRESENT | IDT_INTERRUPT_GATE | IDT_RING0, 0);
}

void idt_set_trap(uint8_t index, uint64_t handler)
{
    idt_set_gate(index, handler, GDT_KERNEL_CODE_SEL,
                    IDT_PRESENT | IDT_TRAP_GATE | IDT_RING0, 0);
}

void idt_set_syscall(uint8_t index, uint64_t handler) 
{
    idt_set_gate(index, handler, GDT_KERNEL_CODE_SEL,
                    IDT_PRESENT | IDT_TRAP_GATE | IDT_RING3, 0);
}

extern void isr_stub_default(void);
extern void isr_divide_error(void);
extern void isr_invalid_opcode(void);
extern void isr_general_protection(void);
extern void isr_page_fault(void);
extern void isr_double_fault(void);

void idt_init(void) {
    // default handler for all entries
    for (int i = 0; i < IDT_SIZE; i++)
        idt_set_handler(i, (uint64_t)isr_stub_default);

    // CPU exceptions
    idt_set_trap(0, (uint64_t)isr_divide_error);
    idt_set_trap(6,  (uint64_t)isr_invalid_opcode);
    idt_set_trap(13, (uint64_t)isr_general_protection);
    idt_set_trap(14, (uint64_t)isr_page_fault);

    // double fault on IST1
    idt_set_gate(8, (uint64_t)isr_double_fault, GDT_KERNEL_CODE_SEL,
                    IDT_PRESENT | IDT_INTERRUPT_GATE | IDT_RING0, 1);

    IDTR idtr = {
        .limit = (uint16_t)(sizeof(idt_table) - 1),
        .base = (uint64_t)&idt_table,
    };
    idt_flush(&idtr);
}