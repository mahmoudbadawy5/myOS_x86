#ifndef _STDIO_H
#define _STDIO_H

#include <stdarg.h>
#include <types.h>
#include <fs/vfs.h>

#define BUFFER_SIZE 1024

size_t print_uint(char *buf, unsigned int val, int base, int size);
size_t print_int(char *buf, int val, int base, int size);
void panic(const char *format, ...);
void vsprintf(char *buf, const char *format, va_list args);
void vfprintf(fs_node_t *file, const char *format, va_list args);
void printf(const char *format, ...);

#endif