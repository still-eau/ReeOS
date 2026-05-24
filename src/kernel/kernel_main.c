
// ReeOS - Kernel module loader orchestrator

#include <stdint.h>
#include <stddef.h>
#include "utils/logger.h"
#include "arch/x86_64/gdt.h"
#include "arch/x86_64/idt.h"
#include "arch/x86_64/pic.h"
#include "arch/x86_64/interrupts.h"
#include "arch/x86_64/pit.h"
#include "mm/pmm.h"

// Minimal VGA text-mode helper
#define VGA_BASE    ((volatile uint16_t *)0xB8000)
#define VGA_COLS    80
#define VGA_ROWS    25

static void vga_clear(uint8_t attr)
{
    volatile uint16_t *vga = VGA_BASE;
    uint16_t blank = (uint16_t)((attr << 8) | ' ');
    for (int i = 0; i < VGA_COLS * VGA_ROWS; i++)
        vga[i] = blank;
}

static void vga_write_string(int row, int col, uint8_t attr, const char *s)
{
    volatile uint16_t *vga = VGA_BASE + row * VGA_COLS + col;
    while (*s)
        *vga++ = (uint16_t)((attr << 8) | (uint8_t)*s++);
}

// Interface registry so modules can talk to each other
#define MAX_INTERFACES 16

typedef struct {
    const char *name;
    void *ptr;
} KernelInterface;

static KernelInterface interface_registry[MAX_INTERFACES];
static int interface_count = 0;

static int mystrcmp(const char *s1, const char *s2)
{
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

void kernel_register_interface(const char *name, void *ptr)
{
    if (interface_count < MAX_INTERFACES) {
        interface_registry[interface_count].name = name;
        interface_registry[interface_count].ptr = ptr;
        interface_count++;
    }
}

void *kernel_get_interface(const char *name)
{
    for (int i = 0; i < interface_count; i++) {
        if (mystrcmp(interface_registry[i].name, name) == 0) {
            return interface_registry[i].ptr;
        }
    }
    return NULL;
}

// Service interfaces for decoupled modules
typedef struct {
    void (*log_debug)(const char *fmt, ...);
    void (*log_info)(const char *fmt, ...);
    void (*log_warn)(const char *fmt, ...);
    void (*log_error)(const char *fmt, ...);
    void (*panic)(const char *fmt, ...);
} LoggerInterface;

typedef struct {
    void (*remap)(uint8_t master_offset, uint8_t slave_offset);
    void (*mask_all)(void);
    void (*unmask_irq)(uint8_t irq);
    void (*eoi)(uint8_t irq);
} PicInterface;

typedef struct {
    void (*register_handler)(uint8_t irq, irq_callback_t cb);
} InterruptInterface;

// Init wrappers for kernel modules
static int init_logger_module(void)
{
    logger_init();

    static LoggerInterface log_if = {
        .log_debug = log_debug,
        .log_info = log_info,
        .log_warn = log_warn,
        .log_error = log_error,
        .panic = kernel_panic
    };
    kernel_register_interface("LOGGER", &log_if);
    return 0;
}

static int init_gdt_module(void)
{
    gdt_init();
    return 0;
}

static int init_idt_module(void)
{
    idt_init();
    return 0;
}

static int init_pic_module(void)
{
    pic_remap(PIC_IRQ_BASE_MASTER, PIC_IRQ_BASE_SLAVE);
    pic_mask_all();

    static PicInterface pic_if = {
        .remap = pic_remap,
        .mask_all = pic_mask_all,
        .unmask_irq = pic_unmask_irq,
        .eoi = pic_eoi
    };
    kernel_register_interface("PIC", &pic_if);
    return 0;
}

static int init_interrupts_module(void)
{
    // Enable timer and keyboard interrupts
    pic_unmask_irq(IRQ_TIMER);
    pic_unmask_irq(IRQ_KEYBOARD);

    // Enable CPU hardware interrupts
    __asm__ volatile("sti");

    static InterruptInterface int_if = {
        .register_handler = irq_register_handler
    };
    kernel_register_interface("INTERRUPTS", &int_if);
    return 0;
}

static int init_pit_module(void)
{
    pit_init(100);
    
    static PitInterface pit_if = {
        .get_ticks = pit_get_ticks
    };
    kernel_register_interface("PIT", &pit_if);
    
    return 0;
}

static int init_pmm_module(void)
{
    // We register it in the interface registry
    static PMMInterface pmm_if = {
        .init_pmm = init_pmm,
        .allocate_frame = allocate_frame,
        .free_frame = free_frame,
        .allocate_frame_range = allocate_frame_range,
        .free_frame_range = free_frame_range,
        .dump_pmm = dump_pmm
    };
    kernel_register_interface("PMM", &pmm_if);

    // pmm_test();

    return 0;
}

// Subsystem module definitions
typedef enum {
    MOD_STATE_OFF = 0,
    MOD_STATE_LOADING,
    MOD_STATE_RUNNING,
    MOD_STATE_ERROR
} ModuleState;

typedef struct {
    const char *name;
    const char *description;
    int (*init)(void);
    ModuleState state;
} KernelModule;

static KernelModule modules[] = {
    { "LOGGER",      "VGA logger subsystem",                  init_logger_module,     MOD_STATE_OFF },
    { "GDT",         "Global descriptor table & segments",    init_gdt_module,        MOD_STATE_OFF },
    { "IDT",         "Interrupt descriptor table & traps",    init_idt_module,        MOD_STATE_OFF },
    { "PIC",         "8259 PIC vectors configuration",        init_pic_module,        MOD_STATE_OFF },
    { "INTERRUPTS",  "CPU interrupts enablement & core IRQs", init_interrupts_module, MOD_STATE_OFF },
    { "PIT", "Programmable interval timer", init_pit_module, MOD_STATE_OFF },
    { "PMM", "Physical Memory Manager & Bitmap", init_pmm_module, MOD_STATE_OFF },
};

// Central loader that orchestrates the boot sequence
void kernel_main(void)
{
    // Clear screen with a nice blue background
    vga_clear(0x19);

    // Draw clean title banner
    vga_write_string(1, 4, 0x1E, "====================================================================");
    vga_write_string(2, 4, 0x1F, "            ReeOS v0.1.0 -- x86_64 Kernel Loader                    ");
    vga_write_string(3, 4, 0x1E, "====================================================================");

    vga_write_string(5, 6, 0x1B, "System orchestrator initializing core modules...");

    int total_modules = sizeof(modules) / sizeof(modules[0]);
    for (int i = 0; i < total_modules; i++) {
        modules[i].state = MOD_STATE_LOADING;
        int row = 7 + i;

        int res = modules[i].init();
        
        // Print current loading module
        LoggerInterface *logger = (LoggerInterface *)kernel_get_interface("LOGGER");
        if (logger != NULL) {
            logger->log_debug("-> Loading %s - %s", modules[i].name, modules[i].description);
        } else {
            vga_write_string(row, 6,  0x1F, "-> Loading ");
            vga_write_string(row, 17, 0x1F, modules[i].name);
            vga_write_string(row, 28, 0x17, "- ");
            vga_write_string(row, 30, 0x17, modules[i].description);
        }

        if (res == 0) {
            modules[i].state = MOD_STATE_RUNNING;
            if (logger != NULL) {
                logger->log_info("[%s] loaded successfully [ OK ]", modules[i].name);
            } else {
                vga_write_string(row, 68, 0x1A, "[ OK ]");
            }
        } else {
            modules[i].state = MOD_STATE_ERROR;
            if (logger != NULL) {
                logger->log_error("[%s] failed to initialize [ FAIL ]", modules[i].name);
                logger->panic("CRITICAL ERROR: Failed to load system module!");
            } else {
                vga_write_string(row, 68, 0x1C, "[ FAIL ]");
                vga_write_string(15, 6, 0x1C, "CRITICAL ERROR: Failed to load system module!");
            }
            for (;;) {
                __asm__ volatile("hlt");
            }
        }
    }

    LoggerInterface *logger = (LoggerInterface *)kernel_get_interface("LOGGER");
    if (logger != NULL) {
        logger->log_info("STATUS: Kernel running. Decoupled Interface Registry is active.");
        logger->log_info("Waiting for hardware interrupts (Timer & Keyboard live)...");
    } else {
        vga_write_string(13, 4, 0x1E, "--------------------------------------------------------------------");
        vga_write_string(14, 6, 0x1E, "STATUS: Kernel running. Decoupled Interface Registry is active.");
        vga_write_string(15, 6, 0x1A, "Waiting for hardware interrupts (Timer & Keyboard live)...");
    }

    // Loop forever and yield to interrupts
    // uint64_t next_log_tick = 500; 

    for (;;) {
        // Puts the CPU on pause until the next interruption to save energy
        __asm__ volatile("hlt");

        // We check if 5 seconds have passed
       // if (pit_get_ticks() >= next_log_tick) {
            //LoggerInterface *logger = (LoggerInterface *)kernel_get_interface("LOGGER");
            //if (logger != NULL) {
             //   logger->log_info("Uptime refresh: %d seconds elapsed.", (int)(pit_get_ticks() / 100));
            //}
            
            // Schedules the next log in 5 seconds
           // next_log_tick = pit_get_ticks() + 500;
        //}
   // }
    }
}