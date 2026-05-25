// =============================================================================
//  ReeOS - Interrupt Descriptor Table (IDT) Header
// =============================================================================

#ifndef IDT_H
#define IDT_H

#include <stdint.h>

#define IDT_SIZE 256

// Type_attributes flag bits
#define IDT_PRESENT        0x80
#define IDT_INTERRUPT_GATE 0x0E
#define IDT_TRAP_GATE      0x0F

// Descriptor privilege level
#define IDT_RING0          0x00
#define IDT_RING3          0x60

// Single 16-byte IDT gate descriptor
typedef struct __attribute__((packed))
{
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  type_attributes;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t reserved;

} IDTGate;

// IDTR register image passed to lidt
typedef struct __attribute__((packed))
{
    uint16_t limit;
    uint64_t base;

} IDTR;

void idt_init(void);

void idt_set_gate(
    uint8_t  index,
    uint64_t handler,
    uint16_t selector,
    uint8_t  flags,
    uint8_t  ist
);

#endif