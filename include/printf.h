#ifndef __TFP_PRINTF__
#define __TFP_PRINTF__

/*
 * Kernel printf implementation with support for:
 * - Basic format specifiers: %d, %u, %x, %X, %c, %s
 * - Length modifiers: %l (long), %ll (long long), %z (size_t)
 * - Examples: %zu (size_t), %zx (size_t hex), %lld (long long)
 */

#include <stdarg.h>

void init_printf(void* putp, void (*putf)(void*, char));

void tfp_printf(const char* fmt, ...);
__attribute__((noreturn)) void tfp_panic(const char* fmt, ...);
void tfp_sprintf(char* s, const char* fmt, ...);

void tfp_format(void* putp, void (*putf)(void*, char), const char* fmt, va_list va);
void tfp_error_printf(const char* fmt, ...);

#define printf_err tfp_error_printf
#define panic tfp_panic
#define printf tfp_printf
#define sprintf tfp_sprintf

#endif
