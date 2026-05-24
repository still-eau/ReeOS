#include "pit.h"
#include "interrupts.h"

static inline void outb(uint16_t port, uint8_t value)
{
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// Explicit declaration of the interface registry helper
extern void *kernel_get_interface(const char *name);

// Interface definition matching kernel_main exactly to avoid redefinition issues
typedef struct {
    void (*register_handler)(uint8_t irq, irq_callback_t cb);
} LocalInterruptInterface;

static volatile uint64_t timer_ticks = 0;

// The interrupt handler (matching irq_callback_t signature)
static void pit_callback(unsigned char irq, ISRFrame *frame)
{
    (void)irq;
    (void)frame;
    timer_ticks++;
}

void pit_init(uint32_t frequency)
{
    timer_ticks = 0;

    uint32_t divisor = 1193182 / frequency;

    outb(0x43, 0x36);
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));

    LocalInterruptInterface *int_if = (LocalInterruptInterface *)kernel_get_interface("INTERRUPTS");
    if (int_if != 0) {
        int_if->register_handler(0, pit_callback);
    }
}

uint64_t pit_get_ticks(void)
{
    return timer_ticks;
}