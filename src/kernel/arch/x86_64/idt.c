#include "idt.h"
#include "gdt.h"
#include "../../utils/logger.h"
#include "pic.h"
#include <stdint.h>

static IDTGate idt_table[IDT_SIZE];

static void idt_flush(const IDTR *idtr)
{
    __asm__ volatile("lidt %0" :: "m"(*idtr) : "memory");
}

void idt_set_gate(uint8_t vec, uint64_t handler,
                  uint16_t sel, uint8_t flags, uint8_t ist)
{
    idt_table[vec].offset_low      = (uint16_t)(handler);
    idt_table[vec].offset_mid      = (uint16_t)(handler >> 16);
    idt_table[vec].offset_high     = (uint32_t)(handler >> 32);
    idt_table[vec].selector        = sel;
    idt_table[vec].ist             = ist & 0x07;
    idt_table[vec].type_attributes = flags;
    idt_table[vec].reserved        = 0;
}

// Clear IF on entry to avoid nested interrupts
static void idt_set_handler(uint8_t vec, uint64_t h)
{
    idt_set_gate(vec, h, GDT_KERNEL_CODE_SEL,
                 IDT_PRESENT | IDT_INTERRUPT_GATE | IDT_RING0, 0);
}

// Keep IF unchanged for synchronous exceptions
static void idt_set_trap(uint8_t vec, uint64_t h)
{
    idt_set_gate(vec, h, GDT_KERNEL_CODE_SEL,
                 IDT_PRESENT | IDT_TRAP_GATE | IDT_RING0, 0);
}

// Allow user mode to trigger via INT instruction
static void idt_set_syscall(uint8_t vec, uint64_t h)
{
    idt_set_gate(vec, h, GDT_KERNEL_CODE_SEL,
                 IDT_PRESENT | IDT_TRAP_GATE | IDT_RING3, 0);
}

// Set interrupt gate with a dedicated TSS stack
static void idt_set_ist(uint8_t vec, uint64_t h, uint8_t ist)
{
    idt_set_gate(vec, h, GDT_KERNEL_CODE_SEL,
                 IDT_PRESENT | IDT_INTERRUPT_GATE | IDT_RING0, ist);
}

// ASM stubs defined in isr.asm
extern void isr_stub_default(void);

// CPU Exceptions
extern void isr_divide_error(void);
extern void isr_debug(void);
extern void isr_nmi(void);
extern void isr_breakpoint(void);
extern void isr_overflow(void);
extern void isr_bound_range(void);
extern void isr_invalid_opcode(void);
extern void isr_device_not_avail(void);
extern void isr_double_fault(void);
extern void isr_invalid_tss(void);
extern void isr_segment_not_present(void);
extern void isr_stack_fault(void);
extern void isr_general_protection(void);
extern void isr_page_fault(void);
extern void isr_fpu_error(void);
extern void isr_alignment_check(void);
extern void isr_machine_check(void);
extern void isr_simd_error(void);

// Master PIC IRQs
extern void irq_timer(void);
extern void irq_keyboard(void);
extern void irq_cascade(void);
extern void irq_com2(void);
extern void irq_com1(void);
extern void irq_lpt2(void);
extern void irq_floppy(void);
extern void irq_lpt1(void);

// Slave PIC IRQs
extern void irq_rtc(void);
extern void irq_acpi(void);
extern void irq_stub_a(void);
extern void irq_stub_b(void);
extern void irq_mouse(void);
extern void irq_fpu(void);
extern void irq_primary_ata(void);
extern void irq_secondary_ata(void);

// Syscall handler
extern void isr_syscall(void);

// Helper to log loaded vectors
static void log_idt_entry(uint8_t vec, const char *name, uint8_t ist)
{
    uint64_t addr =
        (uint64_t)idt_table[vec].offset_low        |
        ((uint64_t)idt_table[vec].offset_mid << 16)|
        ((uint64_t)idt_table[vec].offset_high << 32);

    if (ist)
        log_debug("  IDT[%3d] %-28s  @0x%016llx  IST=%d", vec, name, addr, ist);
    else
        log_debug("  IDT[%3d] %-28s  @0x%016llx", vec, name, addr);
}

// Build and load the IDT
void idt_init(void)
{
    // Fill all 256 gates with a default stub
    for (int i = 0; i < IDT_SIZE; i++)
        idt_set_handler(i, (uint64_t)isr_stub_default);

    // Setup CPU Exceptions
    idt_set_trap(0,  (uint64_t)isr_divide_error);
    idt_set_trap(1,  (uint64_t)isr_debug);
    idt_set_trap(3,  (uint64_t)isr_breakpoint);
    idt_set_gate(3,  (uint64_t)isr_breakpoint,
                 GDT_KERNEL_CODE_SEL,
                 IDT_PRESENT | IDT_TRAP_GATE | IDT_RING3, 0);
    idt_set_trap(4,  (uint64_t)isr_overflow);
    idt_set_trap(5,  (uint64_t)isr_bound_range);
    idt_set_trap(6,  (uint64_t)isr_invalid_opcode);
    idt_set_trap(7,  (uint64_t)isr_device_not_avail);
    idt_set_trap(10, (uint64_t)isr_invalid_tss);
    idt_set_trap(11, (uint64_t)isr_segment_not_present);
    idt_set_trap(13, (uint64_t)isr_general_protection);
    idt_set_trap(14, (uint64_t)isr_page_fault);
    idt_set_trap(16, (uint64_t)isr_fpu_error);
    idt_set_trap(17, (uint64_t)isr_alignment_check);
    idt_set_trap(19, (uint64_t)isr_simd_error);

    // Special IST handlers
    idt_set_ist(2,  (uint64_t)isr_nmi,           2);
    idt_set_ist(8,  (uint64_t)isr_double_fault,   1);
    idt_set_ist(12, (uint64_t)isr_stack_fault,    4);
    idt_set_ist(18, (uint64_t)isr_machine_check,  3);

    // Set IRQs handlers
    idt_set_handler(0x20, (uint64_t)irq_timer);
    idt_set_handler(0x21, (uint64_t)irq_keyboard);
    idt_set_handler(0x22, (uint64_t)irq_cascade);
    idt_set_handler(0x23, (uint64_t)irq_com2);
    idt_set_handler(0x24, (uint64_t)irq_com1);
    idt_set_handler(0x25, (uint64_t)irq_lpt2);
    idt_set_handler(0x26, (uint64_t)irq_floppy);
    idt_set_handler(0x27, (uint64_t)irq_lpt1);
    idt_set_handler(0x28, (uint64_t)irq_rtc);
    idt_set_handler(0x29, (uint64_t)irq_acpi);
    idt_set_handler(0x2A, (uint64_t)irq_stub_a);
    idt_set_handler(0x2B, (uint64_t)irq_stub_b);
    idt_set_handler(0x2C, (uint64_t)irq_mouse);
    idt_set_handler(0x2D, (uint64_t)irq_fpu);
    idt_set_handler(0x2E, (uint64_t)irq_primary_ata);
    idt_set_handler(0x2F, (uint64_t)irq_secondary_ata);

    // Set syscall gate
    idt_set_syscall(0x80, (uint64_t)isr_syscall);

    // Load IDTR register
    IDTR idtr = {
        .limit = (uint16_t)(sizeof(idt_table) - 1),
        .base  = (uint64_t)&idt_table,
    };
    idt_flush(&idtr);

    // Show beautiful loading summary
    log_info("IDT loaded : base=0x%016llx  limit=%u", idtr.base, idtr.limit);

    log_debug(" Exceptions CPU");
    log_idt_entry(0,  "#DE divide error",       0);
    log_idt_entry(1,  "#DB debug",               0);
    log_idt_entry(2,  "#NMI non-maskable",       2);
    log_idt_entry(3,  "#BP breakpoint (DPL3)",   0);
    log_idt_entry(4,  "#OF overflow",            0);
    log_idt_entry(5,  "#BR bound range",         0);
    log_idt_entry(6,  "#UD invalid opcode",      0);
    log_idt_entry(7,  "#NM device not avail",    0);
    log_idt_entry(8,  "#DF double fault",        1);
    log_idt_entry(10, "#TS invalid TSS",         0);
    log_idt_entry(11, "#NP seg not present",     0);
    log_idt_entry(12, "#SS stack fault",         4);
    log_idt_entry(13, "#GP general protection",  0);
    log_idt_entry(14, "#PF page fault",          0);
    log_idt_entry(16, "#MF FPU error",           0);
    log_idt_entry(17, "#AC alignment check",     0);
    log_idt_entry(18, "#MC machine check",       3);
    log_idt_entry(19, "#XF SIMD error",          0);

    log_debug("   IRQs PIC   ");
    log_idt_entry(0x20, "IRQ0 timer",          0);
    log_idt_entry(0x21, "IRQ1 keyboard",       0);
    log_idt_entry(0x28, "IRQ8 RTC",            0);
    log_idt_entry(0x2C, "IRQ12 mouse",         0);
    log_idt_entry(0x2E, "IRQ14 ATA primary",   0);
    log_idt_entry(0x2F, "IRQ15 ATA secondary", 0);

    log_debug("   Syscall   ");
    log_idt_entry(0x80, "INT 0x80 syscall", 0);

    log_info("IDT : %d active vectors", IDT_SIZE);
}