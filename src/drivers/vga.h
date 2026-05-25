// =============================================================================
//  ReeOS - VGA text mode driver Header
// =============================================================================

#ifndef VGA_H
#define VGA_H

#include <stdint.h>

// Basic definitions for VGA text mode
#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_MEMORY_BASE ((volatile uint16_t *)0xB8000)

// VGA colors constants
#define VGA_COLOR_BLACK 0
#define VGA_COLOR_BLUE 1
#define VGA_COLOR_GREEN 2
#define VGA_COLOR_CYAN 3
#define VGA_COLOR_RED 4
#define VGA_COLOR_MAGENTA 5
#define VGA_COLOR_BROWN 6
#define VGA_COLOR_LIGHT_GRAY 7
#define VGA_COLOR_DARK_GRAY 8
#define VGA_COLOR_LIGHT_BLUE 9
#define VGA_COLOR_LIGHT_GREEN 10
#define VGA_COLOR_LIGHT_CYAN 11
#define VGA_COLOR_LIGHT_RED 12
#define VGA_COLOR_LIGHT_MAGENTA 13
#define VGA_COLOR_LIGHT_BROWN 14
#define VGA_COLOR_WHITE 15

#define VGA_DEFAULT_COLOR (VGA_COLOR_WHITE | (VGA_COLOR_BLACK << 4))

typedef struct
{
    uint16_t data;
} vga_char_t;

typedef struct
{
    vga_char_t buffer[VGA_WIDTH * VGA_HEIGHT];
    int cursor_x;
    int cursor_y;
    uint8_t color;
} vga_state_t;

void vga_init(void);
void vga_clear(uint8_t color);
void vga_scroll(void);
void vga_newline(void);
void vga_change_color(uint8_t old_color, uint8_t new_color);
void vga_putc(char c);
void vga_puts(const char *s);
void vga_write_at(const char *s, int row, int col, uint8_t color);
void vga_set_cursor(int x, int y);
void vga_get_cursor(int *x, int *y);
void vga_backspace(void);
void vga_clear_line(int y);
void vga_clear_screen(void);

#endif // VGA_H