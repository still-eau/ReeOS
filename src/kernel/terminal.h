// =============================================================================
//  ReeOS - Terminal kernel module Header
// =============================================================================

#ifndef TERMINAL_H
#define TERMINAL_H

#include <stdint.h>

void terminal_init(void);
void terminal_clear(void);
void terminal_puts(const char *s);
void terminal_putc(char c);
void terminal_set_color(uint8_t color);
void terminal_set_cursor(int x, int y);
void terminal_get_cursor(int *x, int *y);
void terminal_backspace(void);
void terminal_clear_line(int y);
void terminal_clear_screen(void);

#endif // TERMINAL_H