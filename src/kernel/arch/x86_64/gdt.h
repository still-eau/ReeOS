#ifndef GDT_H
#define GDT_H

/*
 * ============================================================================
 *  ReeOS - Global Descriptor Table (GDT) Header
 * ============================================================================
 *
 *  Contains:
 *   - GDT structures
 *   - GDT constants
 *   - GDT function declarations
 *
 *  Architecture:
 *      x86_64
 *
 * ============================================================================
 */

#include <stdint.h>

#define GDT_ENTRIES 7 // null, kcode, kdata, udata, ucode, tss x2

// access bytes
#define GDT_ACCESS_CODE_PL0 0x9A
#define GDT_ACCESS_DATA_PL0 0x92
#define GDT_ACCESS_CODE_PL3 0xFA
#define GDT_ACCESS_DATA_PL3 0xF2
#define GDT_ACCESS_TSS 0x89

// Flags (bytes granularity : G / DB / L / AVL / limit[19:16])
#define GDT_FLAGS_64_CODE 0xA0 // G=1 L=1 DB=0
#define GDT_FLAGS_DATA 0xC0 // G=1 DB=1 (limit ignored in 64bit)

// kernel selectors (RPL=0)
#define GDT_KERNEL_CODE_SEL 0x08
#define GDT_KERNEL_DATA_SEL 0x10

// user selectors (RPL=3) orders forced by SYSCALL/SYSRET
#define GDT_USER_DATA_SEL 0x1B // index 3 / RPL=3
#define GDT_USER_CODE_SEL 0x23 // index 4 / RPL=3

#define GDT_TSS_SEL 0x28 // index 5

typedef struct __attribute__((packed))
{
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_mid;
    uint8_t access;
    uint8_t flags_limit_high;
    uint8_t base_high;
} GDTEntry;

typedef struct __attribute__((packed))
{
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_mid;
    uint8_t access;
    uint8_t flags_limit_high;
    uint8_t base_high;
    uint32_t base_upper;
    uint32_t reserved;
} TSSDescriptor;

typedef struct __attribute__((packed))
{
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist[7];
    uint64_t reserved2;
    uint64_t reserved3;
    uint16_t iopb_offset;
} TSS;

typedef struct __attribute__((packed))
{
    uint16_t limit;
    uint64_t base;
} GDTR;

void gdt_init(void);

#endif