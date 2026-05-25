// =============================================================================
//  ReeOS - Physical Memory Manager (PMM) Header
// =============================================================================

#ifndef PMM_H
#define PMM_H

#include <stddef.h>
#include <stdint.h>

// PMM interface
typedef struct
{
    void (*init_pmm)(void *pmm_data, size_t size);
    uint64_t (*allocate_frame)(void);
    void (*free_frame)(uint64_t frame);
    void (*allocate_frames)(uint64_t *frames, size_t num_frames);
    void (*free_frames)(uint64_t *frames, size_t num_frames);
    uint64_t (*allocate_frame_range)(size_t num_frames);
    void (*free_frame_range)(uint64_t start_frame, size_t num_frames);
    void (*dump_pmm)(void);
} PMMInterface;

// PMM functions declaration
void init_pmm(void *pmm_data, size_t size);
uint64_t allocate_frame(void);
void free_frame(uint64_t frame);
void allocate_frames(uint64_t *frames, size_t num_frames);
void free_frames(uint64_t *frames, size_t num_frames);
uint64_t allocate_frame_range(size_t num_frames);
void free_frame_range(uint64_t start_frame, size_t num_frames);
void dump_pmm(void);
void pmm_test(void);

#endif // PMM_H