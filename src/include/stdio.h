#ifndef STDIO_H
#define STDIO_H

#include <stdarg.h>
#include <stddef.h>

int vsnprintf_c(char *const Buffer, const size_t BufferCount, const char *const Format, va_list ArgList);

#endif
