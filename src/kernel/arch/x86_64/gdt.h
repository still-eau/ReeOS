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

// Number of GDT entries
#define GDT_ENTRIES 5

// Segment flags
#define GDT_PRESENT 0x80
#define GDT_RING0 0x00
#define GDT_RING3 0x60
#define GDT_CODE 0x9A
#define GDT_DATA 0x92

// Segment selectors
#define SELECTOR_CODE_64 0x08
#define SELECTOR_DATA_64 0x10

// GDT entry structure

// NOT FINISH I WILL FINISH HIM LATER (i go to eat)