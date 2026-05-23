// Interrupt Descriptor Table (IDT) initialisation for x86_64.
// Builds a 256-entry IDT, installs CPU exception handlers, then loads
// the table with lidt.

#include "idt.h"
#include "gdt.h"
#include <stdint.h>

static IDTGate idt_table[IDT_SIZE];

static void idt_flush(IDTR *idtr)
{
    __asm__ volatile("lidt %0" : : "m"(*idtr) : "memory");
}

void idt_set_gate(uint8_t index, uint64_t handler,
                  uint16_t selector, uint8_t flags, uint8_t ist)
{
    idt_table[index].offset_low      = (uint16_t)(handler);
    idt_table[index].offset_mid      = (uint16_t)(handler >> 16);
    idt_table[index].offset_high     = (uint32_t)(handler >> 32);
    idt_table[index].selector        = selector;
    idt_table[index].ist             = ist & 0x07;
    idt_table[index].type_attributes = flags;
    idt_table[index].reserved        = 0;
}

// interrupt gate: IF is cleared on entry, preventing nested interrupts
static void idt_set_handler(uint8_t index, uint64_t handler)
{
    idt_set_gate(index, handler, GDT_KERNEL_CODE_SEL,
                 IDT_PRESENT | IDT_INTERRUPT_GATE | IDT_RING0, 0);
}

// trap gate: IF is left unchanged, used for synchronous exceptions
static void idt_set_trap(uint8_t index, uint64_t handler)
{
    idt_set_gate(index, handler, GDT_KERNEL_CODE_SEL,
                 IDT_PRESENT | IDT_TRAP_GATE | IDT_RING0, 0);
}

// user-accessible trap gate: DPL=3 so user code can trigger via int <n>
static void idt_set_syscall(uint8_t index, uint64_t handler)
{
    idt_set_gate(index, handler, GDT_KERNEL_CODE_SEL,
                 IDT_PRESENT | IDT_TRAP_GATE | IDT_RING3, 0);
}

// ASM entry points defined in isr.asm
extern void isr_stub_default(void);
extern void isr_divide_error(void);
extern void isr_invalid_opcode(void);
extern void isr_double_fault(void);
extern void isr_general_protection(void);
extern void isr_page_fault(void);

void idt_init(void)
{
    // fill every vector with the default stub so no entry is left empty
    for (int i = 0; i < IDT_SIZE; i++)
        idt_set_handler(i, (uint64_t)isr_stub_default);

    idt_set_trap(0,  (uint64_t)isr_divide_error);
    idt_set_trap(6,  (uint64_t)isr_invalid_opcode);
    idt_set_trap(13, (uint64_t)isr_general_protection);
    idt_set_trap(14, (uint64_t)isr_page_fault);

    // double fault uses IST slot 1 to guarantee a valid stack on entry
    idt_set_gate(8, (uint64_t)isr_double_fault, GDT_KERNEL_CODE_SEL,
                 IDT_PRESENT | IDT_INTERRUPT_GATE | IDT_RING0, 1);

    IDTR idtr = {
        .limit = (uint16_t)(sizeof(idt_table) - 1),
        .base  = (uint64_t)&idt_table,
    };
    idt_flush(&idtr);

    (void)idt_set_syscall;
}