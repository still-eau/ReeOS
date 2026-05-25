// =============================================================================
//  ReeOS - Logger module implementation linked to Terminal subsystem
// =============================================================================

#include "logger.h"
#include "../terminal.h"
#include "../../include/stdio.h"
#include <stdarg.h>
#include <stddef.h>

// Re-implement colors locally to decouple the logger
#define LOG_COLOR(bg, fg) ((uint8_t)((bg) << 4) | (uint8_t)(fg))
#define LC_BLACK   0
#define LC_GREEN   2
#define LC_CYAN    3
#define LC_RED     4
#define LC_YELLOW  14
#define LC_WHITE   15
#define LC_LGRAY   7

static LogLevel log_min_level = LOG_DEBUG;

static const struct {
    const char *prefix;
    uint8_t prefix_color;
    uint8_t msg_color;
} log_meta[] = {
    [LOG_DEBUG] = { "[DEBUG] ", LOG_COLOR(LC_BLACK, LC_CYAN),   LOG_COLOR(LC_BLACK, LC_LGRAY) },
    [LOG_INFO]  = { "[INFO]  ", LOG_COLOR(LC_BLACK, LC_GREEN),  LOG_COLOR(LC_BLACK, LC_WHITE) },
    [LOG_WARN]  = { "[WARN]  ", LOG_COLOR(LC_BLACK, LC_YELLOW), LOG_COLOR(LC_BLACK, LC_YELLOW) },
    [LOG_ERROR] = { "[ERROR] ", LOG_COLOR(LC_BLACK, LC_RED),    LOG_COLOR(LC_BLACK, LC_RED)   },
};

void logger_init(void)
{
    log_min_level = LOG_DEBUG;
}

void logger_clear(void)
{
    terminal_clear_screen();
}

void logger_set_min_level(LogLevel level)
{
    log_min_level = level;
}

static void logger_log(LogLevel level, const char *fmt, va_list ap)
{
    if (level < log_min_level) return;

    // 1. CRITICAL STEP: Retrieve the CURRENT VGA cursor position
    int current_x, current_y;
    terminal_get_cursor(&current_x, &current_y);

    // If by accident a module has reset the cursor in the banner (lines 0 to 5),
    // we force it back into the secure log area.
    if (current_y < 6) {
        current_y = 6;
        current_x = 0;
    }

    // Ensure the terminal starts printing from the correct position
    terminal_set_cursor(current_x, current_y);

    // 2. Output prefix ([INFO], [DEBUG], etc.)
    terminal_set_color(log_meta[level].prefix_color);
    terminal_puts(log_meta[level].prefix);

    // 3. Message formatting
    char buf[512];
    vsnprintf_c(buf, sizeof(buf), fmt, ap);

    // 4. Message output
    terminal_set_color(log_meta[level].msg_color);
    terminal_puts(buf);
    
    // 5. Clean line break
    terminal_putc('\n');
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

// Kernel panic remains autonomous and immediate, freezing the screen in red
__attribute__((noreturn))
void kernel_panic(const char *fmt, ...)
{
    __asm__ volatile ("cli");

    terminal_set_color(LOG_COLOR(LC_RED, LC_WHITE));
    terminal_clear_screen();

    terminal_set_cursor(2, 1);
    terminal_set_color(LOG_COLOR(LC_RED, LC_YELLOW));
    terminal_puts("*** KERNEL PANIC ***\n\n");

    char buf[512];
    va_list ap; va_start(ap, fmt);
    vsnprintf_c(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    terminal_set_color(LOG_COLOR(LC_RED, LC_WHITE));
    terminal_puts("  ");
    terminal_puts(buf);
    terminal_puts("\n\n  System halted.");

    for (;;) {
        __asm__ volatile ("hlt");
    }
}