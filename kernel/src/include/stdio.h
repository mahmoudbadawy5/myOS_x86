#ifndef _STDIO_H
#define _STDIO_H

extern void print_int(int val, int base, int size);
extern void print_uint(unsigned int val, int base, int size);
extern void panic(const char *str);
extern void printf(const char *format, ...);

#endif