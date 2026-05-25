// =============================================================================
//  ReeOS - Global Descriptor Table setup
// =============================================================================

#include "gdt.h"
#include <stdint.h>

// Stack for double-fault handler to avoid crashes
static uint8_t  df_stack[16384] __attribute__((aligned(16)));

static GDTEntry     gdt_table[GDT_ENTRIES];
static TSSDescriptor tss_desc_hi;   // Upper 64 bits of TSS descriptor
static TSS          kernel_tss      __attribute__((aligned(16)));
static GDTR         gdtr;

// Helper to fill a standard GDT entry
static void gdt_set_entry(int i, uint32_t base, uint32_t limit,
                           uint8_t access, uint8_t flags)
{
    gdt_table[i].limit_low        = (uint16_t)(limit & 0xFFFF);
    gdt_table[i].base_low         = (uint16_t)(base  & 0xFFFF);
    gdt_table[i].base_mid         = (uint8_t)((base  >> 16) & 0xFF);
    gdt_table[i].access           = access;
    gdt_table[i].flags_limit_high = (flags & 0xF0) | ((limit >> 16) & 0x0F);
    gdt_table[i].base_high        = (uint8_t)((base  >> 24) & 0xFF);
}

// Helper to fill the split 16-byte TSS descriptor
static void gdt_set_tss(uint64_t base, uint32_t limit)
{
    // Lower 8 bytes go into slot 5
    gdt_table[5].limit_low        = (uint16_t)(limit & 0xFFFF);
    gdt_table[5].base_low         = (uint16_t)(base  & 0xFFFF);
    gdt_table[5].base_mid         = (uint8_t)((base  >> 16) & 0xFF);
    gdt_table[5].access           = GDT_ACCESS_TSS;
    gdt_table[5].flags_limit_high = (limit >> 16) & 0x0F;
    gdt_table[5].base_high        = (uint8_t)((base  >> 24) & 0xFF);

    // Upper 8 bytes go into slot 6
    tss_desc_hi.base_upper = (uint32_t)(base >> 32);
    tss_desc_hi.reserved   = 0;

    uint64_t hi = ((uint64_t)tss_desc_hi.base_upper);
    uint8_t *dst = (uint8_t *)&gdt_table[6];
    uint8_t *src = (uint8_t *)&hi;
    for (int i = 0; i < 8; i++) dst[i] = src[i];
}

// Commit changes by loading GDTR and reloading segment registers
static void gdt_flush(void)
{
    __asm__ volatile(
        "lgdt %[gdtr]               \n\t"

        // Reload CS via a far return trick
        "pushq %[kcode]             \n\t"
        "leaq  .reload_cs(%%rip), %%rax \n\t"
        "pushq %%rax                \n\t"
        "lretq                      \n\t"

        ".reload_cs:                \n\t"
        // Reload all data segment registers
        "movw  %[kdata], %%ax       \n\t"
        "movw  %%ax, %%ds           \n\t"
        "movw  %%ax, %%es           \n\t"
        "movw  %%ax, %%fs           \n\t"
        "movw  %%ax, %%gs           \n\t"
        "movw  %%ax, %%ss           \n\t"
        :
        : [gdtr]  "m"  (gdtr),
          [kcode] "i"  (GDT_KERNEL_CODE_SEL),
          [kdata] "i"  (GDT_KERNEL_DATA_SEL)
        : "rax", "memory"
    );
}

// Load Task Register with TSS selector
static void tss_flush(void)
{
    __asm__ volatile("ltr %0" : : "r"((uint16_t)GDT_TSS_SEL));
}

// Build the Global Descriptor Table
void gdt_init(void)
{
    // Null descriptor
    gdt_set_entry(0, 0, 0, 0, 0);

    // Kernel code descriptor
    gdt_set_entry(1, 0, 0xFFFFF, GDT_ACCESS_CODE_PL0, GDT_FLAGS_64_CODE);

    // Kernel data descriptor
    gdt_set_entry(2, 0, 0xFFFFF, GDT_ACCESS_DATA_PL0, GDT_FLAGS_DATA);

    // User data descriptor (must come before user code for SYSRET)
    gdt_set_entry(3, 0, 0xFFFFF, GDT_ACCESS_DATA_PL3, GDT_FLAGS_DATA);

    // User code descriptor
    gdt_set_entry(4, 0, 0xFFFFF, GDT_ACCESS_CODE_PL3, GDT_FLAGS_64_CODE);

    // Clean and configure the Task State Segment
    for (uint32_t i = 0; i < sizeof(kernel_tss); i++)
        ((uint8_t *)&kernel_tss)[i] = 0;

    // Privilege level 0 stack
    extern uint8_t kernel_stack_top[];
    kernel_tss.rsp0 = (uint64_t)kernel_stack_top;

    // Dedicated stack for double faults
    kernel_tss.ist[0] = (uint64_t)(df_stack + sizeof(df_stack));

    // Disable I/O permission map
    kernel_tss.iopb_offset = (uint16_t)sizeof(TSS);

    gdt_set_tss((uint64_t)&kernel_tss, (uint32_t)(sizeof(TSS) - 1));

    // Set the limits and apply GDT
    gdtr.limit = (uint16_t)(sizeof(gdt_table) - 1);
    gdtr.base  = (uint64_t)&gdt_table;

    gdt_flush();
    tss_flush();
}
