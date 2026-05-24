// ReeOS pmm functions
#include "pmm.h"
#include "../arch/x86_64/interrupts.h"
#include <stddef.h>
#include <stdint.h>

extern void *kernel_get_interface(const char *name);

// constants
#define PMM_FRAME_SIZE 4096 // 4KB frame size

// PMM data
static uint8_t *pmm_bitmap = NULL;
static size_t pmm_bitmap_size = 0;
static uint64_t pmm_total_frames = 0;
static uint64_t pmm_used_frames = 0;

// pmm allocations and frees functions
static void set_frame_bit(uint64_t frame)
{
    pmm_bitmap[frame / 8] |= (1 << (frame % 8));
}

static void clear_frame_bit(uint64_t frame)
{
    pmm_bitmap[frame / 8] &= ~(1 << (frame % 8));
}

static int is_frame_set(uint64_t frame)
{
    return pmm_bitmap[frame / 8] & (1 << (frame % 8));
}

static int is_frame_clear(uint64_t frame)
{
    return !(pmm_bitmap[frame / 8] & (1 << (frame %8)));
}

void init_pmm(void *pmm_data, size_t size)
{
    pmm_bitmap = (uint8_t *)pmm_data;
    pmm_bitmap_size = size;
    pmm_total_frames =  size * 8;
    pmm_used_frames = 0;

    // init all frame as free
    for (size_t i = 0; i < pmm_bitmap_size; i++)
    {
        pmm_bitmap[i] = 0;
    }
}

uint64_t allocate_frame(void)
{
    cli();
    uint64_t frame = 0;
    // find first frame
    for (uint64_t i = 0; i < pmm_total_frames; i++)
    {
        if (is_frame_clear(i))
        {
            frame = i;
            set_frame_bit(i);
            pmm_used_frames++;
            // reenable interrupts
            sti();
            return frame;
        }
    }
    sti();
    return 0;
}

void free_frame(uint64_t frame)
{
    cli();
    if (frame < pmm_total_frames && is_frame_set(frame))
    {
        clear_frame_bit(frame);
        pmm_used_frames--;
    }
    sti();
}

uint64_t allocate_frame_range(size_t num_frames)
{
    cli();
    if (num_frames == 0)
    {
        sti();
        return 0;
    }

    // scan the bitmap to find a contiguous block of free frames
    for (uint64_t i = 0; i < pmm_total_frames; i++)
    {
        int found = 1;
        for (size_t j = 0; j < num_frames; j++)
        {
            if (is_frame_set(i + j))
            {
                found = 0;
                i += j;
                break; 
            }
        }

        if (found)
        {
            // allocate the contiguous range
            for (size_t j = 0; j < num_frames; j++)
            {
                set_frame_bit(i + j);
                pmm_used_frames++;
            }
            sti();
            return i;
        }
    }

    sti();
    return 0;
}

void free_frame_range(uint64_t start_frame, size_t num_frames)
{
    cli();
    for (size_t i = 0; i < num_frames; i++)
    {
        uint64_t current = start_frame + i;
        if (current < pmm_total_frames && is_frame_set(current))
        {
            clear_frame_bit(current);
            pmm_used_frames--;
        }
    }
    sti();
}

void dump_pmm(void)
{
    // logger interface will handle printing this out safely
    void *logger = kernel_get_interface("LOGGER");
    if (logger != NULL)
    {
        typedef struct {
            void (*log_info)(const char *fmt, ...);
        } LocalLogger;
        ((LocalLogger *)logger)->log_info("PMM: Total frames: %d, Used frames: %d, Free frames: %d", 
            (int)pmm_total_frames, (int)pmm_used_frames, (int)(pmm_total_frames - pmm_used_frames));
    }
}

void pmm_test(void)
{
    void *logger_ptr = kernel_get_interface("LOGGER");
    if (logger_ptr == NULL) return;

    typedef struct {
        void (*log_info)(const char *fmt, ...);
        void (*log_warn)(const char *fmt, ...);
    } TestLogger;
    TestLogger *logger = (TestLogger *)logger_ptr;

    logger->log_info("=== STARTING PMM SELF-TEST ===");

    // Simulate 1MB of memory (1024 KB / 4 KB = 256 frames)
    // A bitmap for 256 frames needs 256 / 8 = 32 bytes
    static uint8_t fake_bitmap_buffer[32];
    
    logger->log_info("Initializing PMM with a fake 1MB RAM region (32 bytes bitmap)...");
    init_pmm(fake_bitmap_buffer, 32);
    dump_pmm();

    // Test 1: Single Frame Allocations
    logger->log_info("Test 1: Allocating 3 single frames...");
    uint64_t f1 = allocate_frame();
    uint64_t f2 = allocate_frame();
    uint64_t f3 = allocate_frame();
    
    logger->log_info("Allocated Frame 1 at index: %d", (int)f1);
    logger->log_info("Allocated Frame 2 at index: %d", (int)f2);
    logger->log_info("Allocated Frame 3 at index: %d", (int)f3);
    dump_pmm();

    // Test 2: Freeing a frame
    logger->log_info("Test 2: Freeing Frame 2 (index %d)...", (int)f2);
    free_frame(f2);
    dump_pmm();

    // Test 3: Reallocating after free (should reuse index 1)
    logger->log_info("Test 3: Allocating a new frame (should reuse index %d)...", (int)f2);
    uint64_t f_reuse = allocate_frame();
    logger->log_info("Allocated Frame at index: %d", (int)f_reuse);
    dump_pmm();

    // Test 4: Contiguous range allocation
    logger->log_info("Test 4: Allocating a contiguous block of 5 frames...");
    uint64_t range_start = allocate_frame_range(5);
    logger->log_info("Contiguous block starts at index: %d", (int)range_start);
    dump_pmm();

    // Test 5: Freeing the contiguous range
    logger->log_info("Test 5: Freeing the 5 contiguous frames...");
    free_frame_range(range_start, 5);
    dump_pmm();

    logger->log_info("=== PMM SELF-TEST COMPLETED ===");
}