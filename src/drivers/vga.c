// =============================================================================
//  ReeOS - VGA text mode driver implementation
// =============================================================================

#include "vga.h"
#include <stddef.h>
#include <stdint.h>
#include "../kernel/arch/x86_64/interrupts.h"

// Note on implementation:
// Define the VGA address as a volatile pointer of 16 bits.
// The keyword 'volatile' forces the compiler to write immediately to memory,
// avoiding any dangerous optimizations.
static volatile uint16_t *vga_memory = (volatile uint16_t *)VGA_MEMORY_BASE;

// Simplified global state
static int cursor_x = 0;
static int cursor_y = 6;
static uint8_t current_color = VGA_DEFAULT_COLOR;

// Speed calculation of the linear index
static inline size_t vga_index(int x, int y)
{
    return (size_t)y * VGA_WIDTH + (size_t)x;
}

// Direct write to the video memory
static void vga_write_char_at(char c, int x, int y, uint8_t color)
{
    if (x >= 0 && x < VGA_WIDTH && y >= 0 && y < VGA_HEIGHT)
    {
        vga_memory[vga_index(x,y)] = ((uint16_t)c | ((uint16_t)color << 8));
    }
}

void vga_init(void)
{
    cursor_x = 0;
    cursor_y = 6;
    current_color = VGA_DEFAULT_COLOR;
}

void vga_clear(uint8_t color)
{
    current_color = color;
    // Clear the entire screen
    for (int y = 0; y < VGA_HEIGHT; y++)
    {
        for (int x = 0; x < VGA_WIDTH; x++)
        {
            vga_write_char_at(' ', x, y, color);
        }
    }
    // Move the cursor back to the top
    cursor_x = 0;
    cursor_y = 6;
    sti();
}

void vga_set_cursor(int x, int y)
{
    cursor_x = x;
    cursor_y = y;
}

void vga_scroll(void)
{
    for (int y = 6; y < VGA_HEIGHT - 1; y++)
    {
        for (int x = 0; x < VGA_WIDTH; x++)
        {
            vga_memory[vga_index(x, y)] = vga_memory[vga_index(x, y + 1)];
        }
    }

    // Clear the last line
    for (int x = 0; x < VGA_WIDTH; x++)
    {
        vga_memory[vga_index(x, VGA_HEIGHT - 1)] = ' ' | (current_color << 8);
    }
}

static void vga_putc_unlocked(char c)
{
    switch (c)
    {
    case '\n':
        cursor_x = 0;
        cursor_y++;
        break;
    case '\r':
        cursor_x = 0;
        break;
    case '\t':
        cursor_x = (cursor_x + 4) & ~3;
        break;
    default:
        vga_write_char_at(c, cursor_x, cursor_y, current_color);
        cursor_x++;
        break;
    }

    if (cursor_x >= VGA_WIDTH)
    {
        cursor_x = 0;
        cursor_y++;
    }

    if (cursor_y >= VGA_HEIGHT)
    {
        vga_scroll();
        cursor_y = VGA_HEIGHT - 1;
    }
}

void vga_putc(char c)
{
    cli();
    vga_putc_unlocked(c);
    sti();
}

void vga_puts(const char *s)
{
    cli();
    
    while (*s)
    {
        vga_putc_unlocked(*s++);
    }
    
    sti();
}

void vga_write_at(const char *s, int row, int col, uint8_t color)
{
    cli(); 
    uint8_t old_color = current_color;
    current_color = color;
    vga_set_cursor(col, row);
    
    while (*s)
    {
        vga_putc_unlocked(*s++);
    }
    
    current_color = old_color;
    sti();
}

void vga_newline(void)
{
    vga_putc('\n');
}

void vga_get_cursor(int *x, int *y)
{
    cli();
    if (x) *x = cursor_x;
    if (y) *y = cursor_y;
    sti();
}

void vga_change_color(uint8_t old_color, uint8_t new_color)
{
    (void)old_color;
    cli();
    current_color = new_color;
    sti();
}

void vga_backspace(void)
{
    cli();
    if (cursor_x > 0)
    {
        cursor_x--;
    }
    else if (cursor_y > 6)
    {
        cursor_y--;
        cursor_x = VGA_WIDTH - 1;
    }
    vga_write_char_at(' ', cursor_x, cursor_y, current_color);
    sti();
}

void vga_clear_line(int y)
{
    cli();
    for (int x = 0; x < VGA_WIDTH; x++)
    {
        vga_write_char_at(' ', x, y, current_color);
    }
    sti();
}

void vga_clear_screen(void)
{
    vga_clear(current_color);
}