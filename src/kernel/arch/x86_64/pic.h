// =============================================================================
//  ReeOS - 8259 Programmable Interrupt Controller (PIC) Header
// =============================================================================
//
//  The legacy 8259A PIC ships with IRQ0-7 wired to INT vectors 8-15.
//  That directly conflicts with CPU exception vectors (#DF=8, #MF=16, ...).
//  We must remap both PICs before enabling interrupts (STI).
//
//  After remapping:
//    IRQ0-7  (master PIC)  → INT 32-39  (0x20-0x27)
//    IRQ8-15 (slave  PIC)  → INT 40-47  (0x28-0x2F)
//
// =============================================================================

#ifndef PIC_H
#define PIC_H

#include <stdint.h>

// Remapped IRQ base vectors
#define PIC_IRQ_BASE_MASTER 0x20   // IRQ0  → INT 32
#define PIC_IRQ_BASE_SLAVE  0x28   // IRQ8  → INT 40

// Individual IRQ lines
#define IRQ_TIMER       0
#define IRQ_KEYBOARD    1
#define IRQ_SLAVE_PIC   2
#define IRQ_SERIAL2     3
#define IRQ_SERIAL1     4
#define IRQ_PARALLEL2   5
#define IRQ_FLOPPY      6
#define IRQ_PARALLEL1   7
#define IRQ_RTC         8
#define IRQ_PS2_MOUSE   12
#define IRQ_FPU         13
#define IRQ_ATA_PRI     14
#define IRQ_ATA_SEC     15

void pic_remap(uint8_t master_offset, uint8_t slave_offset);
void pic_mask_all(void);
void pic_unmask_irq(uint8_t irq);
void pic_eoi(uint8_t irq);

#endif // PIC_H
