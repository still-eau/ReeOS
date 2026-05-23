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

#define GDT_ENTRIES 5

// Access bytes
#define GDT_CODE_PL0 0x9A
#define GDT_DATA_PL0 0x92
#define GDT_CODE_PL3 0xFA
#define GDT_DATA_PL3 0xF2

// Segment selectors
#define SELECTOR_CODE_64 0x08
#define SELECTOR_DATA_64 0x10

// Single GDT entry
typedef struct __attribute__((packed))
{
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_mid;
    uint8_t access;
    uint8_t granularity;
    uint8_t base_high;

} GDTEntry;

// GDTR structure used by lgdt
typedef struct __attribute__((packed))
{
    uint16_t limit;
    uint64_t base;

} GDTR;

// Initialize GDT
void gdt_init(void);

#endif