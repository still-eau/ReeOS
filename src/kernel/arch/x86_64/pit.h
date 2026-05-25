// =============================================================================
//  ReeOS - Programmable Interval Timer (PIT) Header
// =============================================================================

#ifndef ARCH_X86_64_PIT_H
#define ARCH_X86_64_PIT_H

#include <stdint.h>

void pit_init(uint32_t frequency);
uint64_t pit_get_ticks(void);

// Service interface for the PIT module
typedef struct {
    uint64_t (*get_ticks)(void);
} PitInterface;

#endif