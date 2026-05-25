// =============================================================================
//  ReeOS - 8259 PIC vectors configuration implementation
// =============================================================================

#include "pic.h"
#include <stdint.h>

#define PIC_MASTER_CMD  0x20
#define PIC_MASTER_DATA 0x21
#define PIC_SLAVE_CMD   0xa0
#define PIC_SLAVE_DATA  0xa1
#define PIC_EOI         0x20

static inline void outb(uint16_t port, uint8_t val)
{
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void pic_remap(uint8_t master_offset, uint8_t slave_offset)
{
    // Start ICW1: Initialize cascaded PICs, Send ICW4
    outb(PIC_MASTER_CMD, 0x11);
    outb(PIC_SLAVE_CMD, 0x11);

    // ICW2: Set interrupt offsets
    outb(PIC_MASTER_DATA, master_offset);
    outb(PIC_SLAVE_DATA, slave_offset);

    // ICW3: Cascade setup (master has slave at IRQ2, slave on IRQ2)
    outb(PIC_MASTER_DATA, 0x04);
    outb(PIC_SLAVE_DATA, 0x02);

    // ICW4: 8086/8088 mode, auto EOI
    outb(PIC_MASTER_DATA, 0x01);
    outb(PIC_SLAVE_DATA, 0x01);
}

void pic_mask_all(void)
{
    outb(PIC_MASTER_DATA, 0xFF);
    outb(PIC_SLAVE_DATA, 0xFF);
}

void pic_unmask_irq(uint8_t irq)
{
    uint16_t port;
    uint8_t val;

    if(irq < 8) {
        port = PIC_MASTER_DATA;
    } else {
        port = PIC_SLAVE_DATA;
        irq -= 8;
    }

    val = inb(port) & ~(1 << irq);
    outb(port, val);
}

void pic_eoi(uint8_t irq)
{
    if(irq >= 8) {
        outb(PIC_SLAVE_CMD, PIC_EOI);
    }
    outb(PIC_MASTER_CMD, PIC_EOI);
}