// ReeOS logger module
#include "../../include/stdio.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

// Config
#define VGA_BASE    ((volatile uint16_t *)0xB8000)
#define VGA_COLS    80
#define VGA_ROWS    25

// VGA colors
#define VGA_COLOR(bg, fg) ((uint8_t)((bg) << 4) | (uint8_t)(fg))

typedef enum {
    VGA_BLACK   = 0,  VGA_BLUE      = 1,  VGA_GREEN  = 2,
    VGA_CYAN    = 3,  VGA_RED       = 4,  VGA_MAGENTA = 5,
    VGA_BROWN   = 6,  VGA_LGRAY     = 7,  VGA_DGRAY  = 8,
    VGA_LBLUE   = 9,  VGA_LGREEN    = 10, VGA_LCYAN  = 11,
    VGA_LRED    = 12, VGA_LMAGENTA  = 13, VGA_YELLOW = 14,
    VGA_WHITE   = 15,
} VGAColor;

// Log levels
typedef enum {
    LOG_DEBUG = 0,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
} LogLevel;

static LogLevel log_min_level = LOG_DEBUG;   // configurable filter

// Internal state
typedef struct {
    uint8_t ch;
    uint8_t color;
} __attribute__((packed)) VGAEntry;

static VGAEntry *const vga_buf = (VGAEntry *)0xB8000;
static uint8_t vga_row = 0;
static uint8_t vga_col = 0;
static uint8_t vga_default_color;

// Internal primitives
static void vga_put_entry_at(char c, uint8_t color, uint8_t col, uint8_t row)
{
    vga_buf[row * VGA_COLS + col] = (VGAEntry){ (uint8_t)c, color };
}

// Scroll the buffer up one line
static void vga_scroll(void)
{
    // Copy lines 1..24 to 0..23
    for (uint8_t r = 0; r < VGA_ROWS - 1; r++) {
        for (uint8_t c = 0; c < VGA_COLS; c++) {
            vga_buf[r * VGA_COLS + c] = vga_buf[(r + 1) * VGA_COLS + c];
        }
    }
    // Clear the last line
    for (uint8_t c = 0; c < VGA_COLS; c++) {
        vga_put_entry_at(' ', vga_default_color, c, VGA_ROWS - 1);
    }
    if (vga_row > 0) vga_row--;
}

static void vga_newline(void)
{
    vga_col = 0;
    if (++vga_row >= VGA_ROWS)
        vga_scroll();
}

// Put a character in the buffer, handles \n and column overflows
static void vga_putchar(char c, uint8_t color)
{
    if (c == '\n') {
        vga_newline();
        return;
    }
    if (c == '\r') {
        vga_col = 0;
        return;
    }
    if (c == '\t') {
        // Tabulation : next multiple of 8
        uint8_t next = (uint8_t)((vga_col + 8) & ~7u);
        while (vga_col < next && vga_col < VGA_COLS)
            vga_put_entry_at(' ', color, vga_col++, vga_row);
        if (vga_col >= VGA_COLS) vga_newline();
        return;
    }
    vga_put_entry_at(c, color, vga_col, vga_row);
    if (++vga_col >= VGA_COLS) vga_newline();
}

// Write a string with a given color
static void vga_write_string(const char *s, uint8_t color)
{
    for (; *s; s++)
        vga_putchar(*s, color);
}

// Public API — initialization & clearing

void logger_init(void)
{
    vga_default_color = VGA_COLOR(VGA_BLACK, VGA_LGRAY);
    vga_row = 0;
    vga_col = 0;
    for (size_t i = 0; i < VGA_COLS * VGA_ROWS; i++)
        vga_buf[i] = (VGAEntry){ ' ', vga_default_color };
}

void logger_clear(void)
{
    logger_init();
}

void logger_set_min_level(LogLevel level)
{
    log_min_level = level;
}

// Public API — low level writing
void logger_write_color(const char *s, uint8_t color)
{
    vga_write_string(s, color);
}

void logger_writef(const char *fmt, ...)
{
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf_c(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    vga_write_string(buf, vga_default_color);
}

// Public API — log levels

// Log levels:
//  prefix     prefix color                 message color
static const struct {
    const char *prefix;
    uint8_t     prefix_color;
    uint8_t     msg_color;
} log_meta[] = {
    [LOG_DEBUG] = { "[DEBUG] ", VGA_COLOR(VGA_BLACK, VGA_CYAN),   VGA_COLOR(VGA_BLACK, VGA_LGRAY)  },
    [LOG_INFO]  = { "[INFO]  ", VGA_COLOR(VGA_BLACK, VGA_LGREEN), VGA_COLOR(VGA_BLACK, VGA_WHITE)  },
    [LOG_WARN]  = { "[WARN]  ", VGA_COLOR(VGA_BLACK, VGA_YELLOW), VGA_COLOR(VGA_BLACK, VGA_YELLOW) },
    [LOG_ERROR] = { "[ERROR] ", VGA_COLOR(VGA_BLACK, VGA_LRED),   VGA_COLOR(VGA_BLACK, VGA_LRED)   },
};

static void logger_log(LogLevel level, const char *fmt, va_list ap)
{
    if (level < log_min_level) return;

    vga_write_string(log_meta[level].prefix, log_meta[level].prefix_color);

    char buf[480];
    vsnprintf_c(buf, sizeof(buf), fmt, ap);
    vga_write_string(buf, log_meta[level].msg_color);
    vga_putchar('\n', vga_default_color);
}

void log_debug(const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt);
    logger_log(LOG_DEBUG, fmt, ap);
    va_end(ap);
}

void log_info(const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt);
    logger_log(LOG_INFO, fmt, ap);
    va_end(ap);
}

void log_warn(const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt);
    logger_log(LOG_WARN, fmt, ap);
    va_end(ap);
}

void log_error(const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt);
    logger_log(LOG_ERROR, fmt, ap);
    va_end(ap);
}

// Panic - red screen, message, halt
__attribute__((noreturn))
void kernel_panic(const char *fmt, ...)
{
    // Fill the entire screen in red
    uint8_t panic_color = VGA_COLOR(VGA_RED, VGA_WHITE);
    for (size_t i = 0; i < VGA_COLS * VGA_ROWS; i++)
        vga_buf[i] = (VGAEntry){ ' ', panic_color };

    vga_row = 1; vga_col = 2;
    vga_write_string("*** KERNEL PANIC ***", VGA_COLOR(VGA_RED, VGA_YELLOW));

    vga_row = 3; vga_col = 2;
    char buf[472];
    va_list ap; va_start(ap, fmt);
    vsnprintf_c(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    vga_write_string(buf, panic_color);

    vga_row = 5; vga_col = 2;
    vga_write_string("System halted.", panic_color);

    // Disable interrupts and loop indefinitely
    __asm__ volatile ("cli");
    for (;;) __asm__ volatile ("hlt");
}