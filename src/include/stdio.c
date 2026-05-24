#include "stdio.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

static void utoa(uint64_t value, char *str, int base, int uppercase)
{
    char buf[64];
    char *ptr = buf;
    
    do {
        uint64_t low = value % base;
        if (low < 10) {
            *ptr++ = (char)(low + '0');
        } else {
            *ptr++ = (char)(low - 10 + (uppercase ? 'A' : 'a'));
        }
        value /= base;
    } while (value);
    
    // Reverse the string
    while (ptr > buf) {
        *str++ = *--ptr;
    }
    *str = '\0';
}

static void format_integer(char *out, size_t *out_idx, size_t max_len, uint64_t val, int is_signed, int base, int width, char pad_char, int left_align, int uppercase)
{
    char num_buf[64];
    int neg = 0;
    
    if (is_signed && (int64_t)val < 0 && base == 10) {
        neg = 1;
        val = (uint64_t)(-(int64_t)val);
    }
    
    utoa(val, num_buf, base, uppercase);
    
    int len = 0;
    while (num_buf[len]) len++;
    
    int total_len = len + neg;
    int pad_len = (width > total_len) ? (width - total_len) : 0;
    
    size_t i = *out_idx;
    
    if (!left_align) {
        if (pad_char == '0') {
            // print sign before zero-padding
            if (neg && i < max_len - 1) {
                out[i++] = '-';
                neg = 0;
            }
            while (pad_len-- > 0 && i < max_len - 1) {
                out[i++] = '0';
            }
        } else {
            // print spaces before sign
            while (pad_len-- > 0 && i < max_len - 1) {
                out[i++] = ' ';
            }
            if (neg && i < max_len - 1) {
                out[i++] = '-';
                neg = 0;
            }
        }
    } else {
        if (neg && i < max_len - 1) {
            out[i++] = '-';
            neg = 0;
        }
    }
    
    // print number characters
    for (int j = 0; j < len && i < max_len - 1; j++) {
        out[i++] = num_buf[j];
    }
    
    // spaces for left-alignment
    if (left_align) {
        while (pad_len-- > 0 && i < max_len - 1) {
            out[i++] = ' ';
        }
    }
    
    *out_idx = i;
}

static void format_string(char *out, size_t *out_idx, size_t max_len, const char *s, int width, int left_align)
{
    if (!s) s = "(null)";
    
    int len = 0;
    while (s[len]) len++;
    
    int pad_len = (width > len) ? (width - len) : 0;
    
    size_t i = *out_idx;
    
    if (!left_align) {
        while (pad_len-- > 0 && i < max_len - 1) {
            out[i++] = ' ';
        }
    }
    
    for (int j = 0; j < len && i < max_len - 1; j++) {
        out[i++] = s[j];
    }
    
    if (left_align) {
        while (pad_len-- > 0 && i < max_len - 1) {
            out[i++] = ' ';
        }
    }
    
    *out_idx = i;
}

int vsnprintf_c(char *const Buffer, const size_t BufferCount, const char *const Format, va_list ArgList)
{
    size_t i = 0;
    const char *fmt = Format;
    
    while (*fmt && i < BufferCount - 1) {
        if (*fmt == '%') {
            fmt++; // skip '%'
            
            if (*fmt == '%') {
                Buffer[i++] = '%';
                fmt++;
                continue;
            }
            
            // flags
            int left_align = 0;
            char pad_char = ' ';
            
            while (*fmt == '-' || *fmt == '0') {
                if (*fmt == '-') left_align = 1;
                if (*fmt == '0') pad_char = '0';
                fmt++;
            }
            
            // width
            int width = 0;
            while (*fmt >= '0' && *fmt <= '9') {
                width = width * 10 + (*fmt - '0');
                fmt++;
            }
            
            // length modifier
            int is_long = 0;
            if (*fmt == 'l') {
                is_long = 1;
                fmt++;
                if (*fmt == 'l') {
                    is_long = 2;
                    fmt++;
                }
            }
            
            char spec = *fmt;
            if (spec == '\0') break;
            
            if (spec == 's') {
                const char *s = va_arg(ArgList, const char *);
                format_string(Buffer, &i, BufferCount, s, width, left_align);
            }
            else if (spec == 'c') {
                char c = (char)va_arg(ArgList, int);
                char c_str[2] = { c, '\0' };
                format_string(Buffer, &i, BufferCount, c_str, width, left_align);
            }
            else if (spec == 'd') {
                int64_t val;
                if (is_long == 2) {
                    val = va_arg(ArgList, long long);
                } else if (is_long == 1) {
                    val = va_arg(ArgList, long);
                } else {
                    val = va_arg(ArgList, int);
                }
                format_integer(Buffer, &i, BufferCount, (uint64_t)val, 1, 10, width, pad_char, left_align, 0);
            }
            else if (spec == 'u') {
                uint64_t val;
                if (is_long == 2) {
                    val = va_arg(ArgList, unsigned long long);
                } else if (is_long == 1) {
                    val = va_arg(ArgList, unsigned long);
                } else {
                    val = va_arg(ArgList, unsigned int);
                }
                format_integer(Buffer, &i, BufferCount, val, 0, 10, width, pad_char, left_align, 0);
            }
            else if (spec == 'x' || spec == 'X') {
                uint64_t val;
                if (is_long == 2) {
                    val = va_arg(ArgList, unsigned long long);
                } else if (is_long == 1) {
                    val = va_arg(ArgList, unsigned long);
                } else {
                    val = va_arg(ArgList, unsigned int);
                }
                format_integer(Buffer, &i, BufferCount, val, 0, 16, width, pad_char, left_align, spec == 'X');
            }
            else {
                // fallback
                Buffer[i++] = '%';
                if (i < BufferCount - 1) {
                    Buffer[i++] = spec;
                }
            }
            
            fmt++;
        } else {
            Buffer[i++] = *fmt++;
        }
    }
    
    Buffer[i] = '\0';
    return (int)i;
}