// ReeOS - Interrupt and Exception handlers

#include "interrupts.h"
#include "../../utils/logger.h"
#include <stdint.h>

static inline uint64_t read_cr2(void)
{
    uint64_t val;
    __asm__ volatile("mov %%cr2, %0" : "=r"(val));
    return val;
}

static inline uint64_t read_cr3(void)
{
    uint64_t val;
    __asm__ volatile("mov %%cr3, %0" : "=r"(val));
    return val;
}

// Dump the full frame to the VGA screen on fatal errors
static void dump_frame(const ISRFrame *f)
{
    log_error("   Register dump   ");
    log_error("  VEC=%llu  ERR=0x%016llx", f->vec, f->err);
    log_error("  RIP=0x%016llx  CS=0x%04llx  RFLAGS=0x%016llx",
              f->rip, f->cs, f->rflags);
    log_error("  RSP=0x%016llx  SS=0x%04llx", f->rsp, f->ss);
    log_error("  RAX=0x%016llx  RBX=0x%016llx", f->rax, f->rbx);
    log_error("  RCX=0x%016llx  RDX=0x%016llx", f->rcx, f->rdx);
    log_error("  RSI=0x%016llx  RDI=0x%016llx", f->rsi, f->rdi);
    log_error("  RBP=0x%016llx  R8 =0x%016llx", f->rbp, f->r8);
    log_error("  R9 =0x%016llx  R10=0x%016llx", f->r9,  f->r10);
    log_error("  R11=0x%016llx  R12=0x%016llx", f->r11, f->r12);
    log_error("  R13=0x%016llx  R14=0x%016llx", f->r13, f->r14);
    log_error("  R15=0x%016llx", f->r15);
}

typedef void (*irq_callback_t)(uint8_t irq, ISRFrame *frame);

static irq_callback_t irq_handlers[16] = { 0 };

void irq_register_handler(uint8_t irq, irq_callback_t cb)
{
    if (irq < 16)
        irq_handlers[irq] = cb;
}

#define PIC_MASTER_CMD  0x20
#define PIC_SLAVE_CMD   0xA0
#define PIC_READ_ISR    0x0B

static inline uint8_t pic_read_isr(uint16_t port)
{
    __asm__ volatile("outb %0, %1" :: "a"((uint8_t)PIC_READ_ISR), "Nd"(port));
    uint8_t val;
    __asm__ volatile("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

// Ignore cheap spurious IRQ signals
static int is_spurious(uint8_t irq)
{
    if (irq == 7 && !(pic_read_isr(PIC_MASTER_CMD) & (1 << 7))) return 1;
    if (irq == 15 && !(pic_read_isr(PIC_SLAVE_CMD)  & (1 << 7))) return 1;
    return 0;
}

// CPU Exception handlers
void c_isr_default(ISRFrame *f)
{
    dump_frame(f);
    kernel_panic("#UNK unhandled interrupt vector %llu", f->vec);
}

void c_isr_divide_error(ISRFrame *f)
{
    dump_frame(f);
    kernel_panic("#DE divide-by-zero  RIP=0x%016llx", f->rip);
}

void c_isr_debug(ISRFrame *f)
{
    log_warn("#DB debug trap  RIP=0x%016llx  RFLAGS=0x%016llx",
             f->rip, f->rflags);
}

void c_isr_nmi(ISRFrame *f)
{
    // NMIs are unrecoverable hardware faults. Log and spin.
    dump_frame(f);
    log_error("#NMI non-maskable interrupt - halting");
    __asm__ volatile("cli");
    for (;;) __asm__ volatile("hlt");
}

void c_isr_breakpoint(ISRFrame *f)
{
    log_debug("#BP breakpoint  RIP=0x%016llx", f->rip);
}

void c_isr_overflow(ISRFrame *f)
{
    dump_frame(f);
    kernel_panic("#OF integer overflow  RIP=0x%016llx", f->rip);
}

void c_isr_bound_range(ISRFrame *f)
{
    dump_frame(f);
    kernel_panic("#BR bound range exceeded  RIP=0x%016llx", f->rip);
}

void c_isr_invalid_opcode(ISRFrame *f)
{
    dump_frame(f);
    kernel_panic("#UD invalid opcode  RIP=0x%016llx", f->rip);
}

void c_isr_device_not_avail(ISRFrame *f)
{
    dump_frame(f);
    kernel_panic("#NM device not available (FPU)  RIP=0x%016llx", f->rip);
}

void c_isr_double_fault(ISRFrame *f)
{
    dump_frame(f);
    kernel_panic("#DF double fault - unrecoverable");
}

void c_isr_invalid_tss(ISRFrame *f)
{
    dump_frame(f);
    kernel_panic("#TS invalid TSS  selector=0x%llx  RIP=0x%016llx",
                 f->err, f->rip);
}

void c_isr_segment_not_present(ISRFrame *f)
{
    dump_frame(f);
    kernel_panic("#NP segment not present  selector=0x%llx  RIP=0x%016llx",
                 f->err, f->rip);
}

void c_isr_stack_fault(ISRFrame *f)
{
    dump_frame(f);
    kernel_panic("#SS stack fault  selector=0x%llx  RIP=0x%016llx",
                 f->err, f->rip);
}

void c_isr_general_protection(ISRFrame *f)
{
    dump_frame(f);
    if (f->err)
        kernel_panic("#GP general protection  selector=0x%llx  RIP=0x%016llx",
                     f->err, f->rip);
    else
        kernel_panic("#GP general protection (no selector)  RIP=0x%016llx",
                     f->rip);
}

void c_isr_page_fault(ISRFrame *f)
{
    uint64_t fault_addr = read_cr2();
    uint64_t cr3        = read_cr3();

    const char *cause = (f->err & 1) ? "protection"   : "not-present";
    const char *op    = (f->err & 2) ? "write"        : "read";
    const char *ring  = (f->err & 4) ? "user"         : "kernel";
    const char *nx    = (f->err & 16)? " [NX]"        : "";

    dump_frame(f);
    log_error("#PF %s %s from %s%s", cause, op, ring, nx);
    log_error("  fault addr  = 0x%016llx", fault_addr);
    log_error("  CR3         = 0x%016llx", cr3);
    log_error("  error code  = 0x%016llx", f->err);

    kernel_panic("#PF @ 0x%016llx (%s %s %s%s)",
                 fault_addr, cause, op, ring, nx);
}

void c_isr_fpu_error(ISRFrame *f)
{
    dump_frame(f);
    kernel_panic("#MF x87 FPU error  RIP=0x%016llx", f->rip);
}

void c_isr_alignment_check(ISRFrame *f)
{
    dump_frame(f);
    kernel_panic("#AC alignment check  RIP=0x%016llx  CS=0x%llx",
                 f->rip, f->cs);
}

void c_isr_machine_check(ISRFrame *f)
{
    // Machine check is completely fatal. Halt now.
    (void)f;
    log_error("#MC machine check - unrecoverable hardware error");
    __asm__ volatile("cli");
    for (;;) __asm__ volatile("hlt");
}

void c_isr_simd_error(ISRFrame *f)
{
    dump_frame(f);
    kernel_panic("#XF SIMD floating-point exception  RIP=0x%016llx", f->rip);
}

// Master IRQ entrypoint called from ASM stubs
void c_irq_handler(ISRFrame *f, uint64_t irq_num)
{
    if (irq_num >= 16) {
        log_warn("IRQ out of range : %llu (vec=0x%llx)", irq_num, f->vec);
        return;
    }

    if (is_spurious((uint8_t)irq_num)) {
        log_warn("Spurious IRQ%llu ignored", irq_num);
        return;
    }

    if (irq_handlers[irq_num])
        irq_handlers[irq_num]((uint8_t)irq_num, f);
}

// Minimal syscall handler for INT 0x80
void c_syscall_handler(ISRFrame *f)
{
    uint64_t syscall_nr = f->rax;

    log_debug("syscall %llu  RIP=0x%016llx  CS=0x%llx",
              syscall_nr, f->rip, f->cs);

    switch (syscall_nr) {
    case 0:   // SYS_read placeholder
        f->rax = (uint64_t)-1;
        break;
    case 1:   // SYS_write placeholder
        f->rax = (uint64_t)-1;
        break;
    default:
        log_warn("Unknown syscall %llu", syscall_nr);
        f->rax = (uint64_t)-38;
        break;
    }
}

// cli and sti
void cli(void)
{
    __asm__ volatile("cli");
}

void sti(void)
{
    __asm__ volatile("sti");
}