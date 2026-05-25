// =============================================================================
//  ReeOS - Kernel module loader orchestrator
// =============================================================================

#include <stdint.h>
#include <stddef.h>
#include "terminal.h"
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
    // Unmask hardware lines on PIC
    pic_unmask_irq(IRQ_TIMER);
    pic_unmask_irq(IRQ_KEYBOARD);

    // FIX: "sti" instruction has been removed from here to avoid 
    // an IRQ trigger before the complete configuration of the PIT.

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
    static PMMInterface pmm_if = {
        .init_pmm = init_pmm,
        .allocate_frame = allocate_frame,
        .free_frame = free_frame,
        .allocate_frame_range = allocate_frame_range,
        .free_frame_range = free_frame_range,
        .dump_pmm = dump_pmm
    };

    // pmm_test();
    kernel_register_interface("PMM", &pmm_if);
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
    { "PIT",         "Programmable interval timer",           init_pit_module,        MOD_STATE_OFF },
    { "PMM",         "Physical Memory Manager & Bitmap",      init_pmm_module,        MOD_STATE_OFF },
};

// Central loader that orchestrates the boot sequence
void kernel_main(void)
{
    terminal_init(); 
    vga_clear(0x19);

    // Draw the banner
    vga_write_string(1, 4, 0x1E, "====================================================================");
    vga_write_string(2, 4, 0x1F, "            ReeOS v0.1.0 -- x86_64 Kernel Loader                    ");
    vga_write_string(3, 4, 0x1E, "====================================================================");
    vga_write_string(5, 6, 0x1B, "System orchestrator initializing core modules...");

    // ---------------------------------------------------------
    // SECURITY TEST 1: Do we reach here?
    // ---------------------------------------------------------
    vga_write_string(7, 6, 0x1A, "DEBUG: About to enter module loop...");

    int total_modules = sizeof(modules) / sizeof(modules[0]);
    
    for (int i = 0; i < total_modules; i++) {
        modules[i].state = MOD_STATE_LOADING;

        // security test
        vga_write_string(8 + i, 6, 0x1E, "DEBUG: Init module: ");
        vga_write_string(8 + i, 26, 0x1F, modules[i].name);

        int res = modules[i].init();
        
        vga_write_string(8 + i, 40, 0x1A, "[DONE]"); // display if init is not crash

        LoggerInterface *logger = (LoggerInterface *)kernel_get_interface("LOGGER");
        if (logger != NULL) {
            logger->log_debug("Loading %s", modules[i].name);
        } else {
            vga_write_string(6, 6, 0x1F, "-> Bootstrapping LOGGER subsystem...");
        }

        if (res == 0) {
            modules[i].state = MOD_STATE_RUNNING;
            if (logger != NULL) {
                logger->log_info("[%s] loaded successfully [ OK ]", modules[i].name);
            } else {
                vga_write_string(6, 68, 0x1A, "[ OK ]");
            }
        } else {
            modules[i].state = MOD_STATE_ERROR;
            if (logger != NULL) {
                logger->log_error("[%s] failed to initialize [ FAIL ]", modules[i].name);
                logger->panic("CRITICAL ERROR: Failed to load system module!");
            } else {
                vga_write_string(6, 68, 0x1C, "[ FAIL ]");
                // simulate raw hardware panic if logger is not operational
                vga_write_string(15, 6, 0x1C, "CRITICAL HARDWARE FAILURE DURING BOOT");
            }
            
            // safety lock in case of critical failure
            for (;;) {
                __asm__ volatile("hlt");
            }
        }
    }

    // status of the kernel and opening of CPU hardware interrupts
    LoggerInterface *logger = (LoggerInterface *)kernel_get_interface("LOGGER");
    
    if (logger != NULL) {
        logger->log_info("STATUS: Kernel running. Decoupled Interface Registry is active.");
        logger->log_info("Enabling CPU hardware interrupts safely...");
    }

    // critical point : the shield is up, the IDT and the PIT are synchronized.
    __asm__ volatile("sti");

    if (logger != NULL) {
        logger->log_info("Waiting for hardware interrupts (Timer & Keyboard live)...");
    }

    // main loop of the kernel (hardware awakening via PIT)
    uint64_t next_log_tick = 100; // 100 ticks = 1 second at 100Hz frequency

    for (;;) {
        // CPU goes to sleep in low power mode waiting for the next clock signal
        __asm__ volatile("hlt");

        // As soon as the clock interrupt (INT 0x20) wakes up the CPU, we check the Uptime
        if (pit_get_ticks() >= next_log_tick) {
            
            logger = (LoggerInterface *)kernel_get_interface("LOGGER");
            if (logger != NULL) {
                // writes the uptime in a smooth way in the sliding flow of the terminal
                logger->log_info("Uptime: %d seconds.", (int)(pit_get_ticks() / 100));
            }
            
            // Schedule the next display event (+1 second)
            next_log_tick = pit_get_ticks() + 100;
        }
    }
}