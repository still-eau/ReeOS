// =============================================================================
//  ReeOS - Logger module Header
// =============================================================================

#pragma once
#include <stdint.h>

typedef enum { LOG_DEBUG = 0, LOG_INFO, LOG_WARN, LOG_ERROR } LogLevel;

void logger_init(void);
void logger_clear(void);
void logger_set_min_level(LogLevel level);

void log_debug(const char *fmt, ...);
void log_info (const char *fmt, ...);
void log_warn (const char *fmt, ...);
void log_error(const char *fmt, ...);

__attribute__((noreturn)) void kernel_panic(const char *fmt, ...);