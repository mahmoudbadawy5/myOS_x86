#ifndef _STDIO_H
#define _STDIO_H

#include <stdarg.h>

extern void print_int(int val, int base, int size);
extern void print_uint(unsigned int val, int base, int size);
extern void panic(const char *format, ...);
extern void vprintf(const char *format, va_list args);
extern void printf(const char *format, ...);

#endif