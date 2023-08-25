#ifndef _STDIO_H
#define _STDIO_H

#include <stdarg.h>
#include <types.h>
#include <fs/vfs.h>

#define BUFFER_SIZE 1024
#define MAX_FILES 256

#define FILE_READ 0x01
#define FILE_WRITE 0x02
#define FILE_APPEND 0x04

typedef struct __sFILE
{
    uint8_t flags;
    fs_node_t *file;
} FILE;

FILE **files_open;

int fopen(char *path, char *modes);
size_t fread(char *buf, size_t size, size_t count, FILE *fp);
size_t fwrite(const char *buf, size_t size, size_t count, FILE *fp);

size_t
print_uint(char *buf, unsigned int val, int base, int size);
size_t print_int(char *buf, int val, int base, int size);
void panic(const char *format, ...);
void vsprintf(char *buf, const char *format, va_list args);
void vfprintf(fs_node_t *file, const char *format, va_list args);
void printf(const char *format, ...);

#endif