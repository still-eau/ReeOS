// =============================================================================
//  ReeOS - Terminal kernel module implementation
// =============================================================================

#include "terminal.h"
#include "../drivers/vga.h"

extern void *kernel_get_interface(const char *name);

// Interface definition matching kernel_main exactly to avoid redefinition issues
typedef struct {
    void (*init)(void);
    void (*clear)(void);
    void (*puts)(const char *s);
    void (*putc)(char c);
    void (*set_color)(uint8_t color);
    void (*set_cursor)(int x, int y);
    void (*get_cursor)(int *x, int *y);
    void (*backspace)(void);
    void (*clear_line)(int y);
    void (*clear_screen)(void);
} TerminalInterface;

static TerminalInterface terminal_if = {
    .init = terminal_init,
    .clear = terminal_clear,
    .puts = terminal_puts,
    .putc = terminal_putc,
    .set_color = terminal_set_color,
    .set_cursor = terminal_set_cursor,
    .get_cursor = terminal_get_cursor,
    .backspace = terminal_backspace,
    .clear_line = terminal_clear_line,
    .clear_screen = terminal_clear_screen
};

void terminal_init(void)
{
    vga_init();
}

void terminal_clear(void)
{
    vga_clear(VGA_DEFAULT_COLOR);
}

void terminal_puts(const char *s)
{
    vga_puts(s);
}

void terminal_putc(char c)
{
    vga_putc(c);
}

void terminal_set_color(uint8_t color)
{
    vga_change_color(VGA_DEFAULT_COLOR, color);
}

void terminal_set_cursor(int x, int y)
{
    vga_set_cursor(x, y);
}

void terminal_get_cursor(int *x, int *y)
{
    vga_get_cursor(x, y);
}

void terminal_backspace(void)
{
    vga_backspace();
}

void terminal_clear_line(int y)
{
    vga_clear_line(y);
}

void terminal_clear_screen(void)
{
    vga_clear_screen();
}