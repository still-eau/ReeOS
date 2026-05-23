#ifndef IDT_H
#define IDT_H

/**
 * ============================================================================
 *  ReeOS - Interrupt Descriptor Table (IDT) Header
 * ============================================================================
 *
 *  Contains:
 *   - IDT structures
 *   - IDT constants
 *   - IDT function declarations
 *
 *  Architecture:
 *      x86_64
 *
 * ============================================================================
 */

#include <stdint.h>

#define IDT_SIZE 256

#define IDT_PRESENT        0x80
#define IDT_INTERRUPT_GATE 0x0E
#define IDT_TRAP_GATE      0x0F

#define IDT_RING0          0x00
#define IDT_RING3          0x60

// Single IDT entry
typedef struct __attribute__((packed))
{
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t type_attributes;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t reserved;

} IDTGate;

// IDTR structure used by lidt
typedef struct __attribute__((packed))
{
    uint16_t limit;
    uint64_t base;

} IDTR;

// Initialize the IDT
void idt_init(void);

// Configure an IDT gate
void idt_set_gate(
    uint8_t index,
    uint64_t handler,
    uint16_t selector,
    uint8_t flags,
    uint8_t ist
);

// Load IDTR register (ASM)
extern void idt_flush(IDTR* idtr);

#endif