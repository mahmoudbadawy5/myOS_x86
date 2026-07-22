#include <vga.h>
#include <stdio.h>
#include <mem/phys_mem.h>
#include <stdarg.h>
#include <types.h>
#include <string.h>

size_t print_uint(char *buf, unsigned int val, int base, int size)
{
    int cursz = 0;
    while (val)
    {
        char curd = val % base;
        val /= base;
        curd += (curd <= 9 ? '0' : 'A' - 10);
        buf[cursz++] = curd;
    }
    while (cursz < size)
        buf[cursz++] = '0';
    // Reverse string
    for (int i = 0; i < cursz / 2; i++)
    {
        char tmp = buf[i];
        buf[i] = buf[cursz - i - 1];
        buf[cursz - i - 1] = tmp;
    }
    return cursz;
}

size_t print_int(char *buf, int val, int base, int size)
{
    int off = 0;
    if (val < 0)
    {
        buf[0] = '-';
        off = 1;
        val *= -1;
    }
    int cursz = 0;
    while (val)
    {
        char curd = val % base;
        val /= base;
        curd += (curd <= 9 ? '0' : 'A' - 10);
        buf[off + cursz++] = curd;
    }
    while (cursz < size)
        buf[off + cursz++] = '0';
    // Reverse string
    for (int i = 0; i < cursz / 2; i++)
    {
        char tmp = buf[off + i];
        buf[off + i] = buf[off + cursz - i - 1];
        buf[off + cursz - i - 1] = tmp;
    }
    return off + cursz;
}

void panic(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    char buf[BUFFER_SIZE];
    puts("\x1b\x40Kernel Panic: ");
    vsnprintf(buf, BUFFER_SIZE, format, args);
    puts(buf);
    va_end(args);
    __asm__("cli; hlt;");
}

void vsnprintf(char *buf, uint32_t bufsize, const char *format, va_list args)
{
    if (bufsize == 0) return;
    uint32_t pos = 0;
    uint32_t limit = bufsize - 1;
    for (int i = 0; format[i]; i++)
    {
        if (format[i] != '%')
        {
            if (pos < limit) buf[pos++] = format[i];
            continue;
        }
        i++;
        int sz = 1, base = 10;
        uint32_t uval;
        int32_t ival;
        char *sval;
        if (format[i] == '0')
        {
            sz = 0;
            while ('0' <= format[i] && format[i] <= '9')
            {
                sz *= 10;
                sz += format[i] - '0';
                i++;
            }
        }
        switch (format[i])
        {
        case '%':
            if (pos < limit) buf[pos++] = '%';
            break;
        case 'd':
            ival = va_arg(args, int32_t);
            if (pos < limit) pos += print_int(buf + pos, ival, 10, sz);
            break;
        case 'x':
            ival = va_arg(args, int32_t);
            pos += print_uint(buf + pos, (uint32_t)ival, 16, sz);
            break;
        case 'u':
            if (format[i + 1] == 'x')
                base = 16, i++;
            else if (format[i + 1] == 'd')
                base = 10, i++;
            else
                base = 10;
            uval = va_arg(args, uint32_t);
            pos += print_uint(buf + pos, uval, base, sz);
            break;
        case 's':
            sval = va_arg(args, char *);
            while (*sval)
            {
                if (pos < limit) buf[pos++] = *sval;
                sval++;
            }
            break;
        case 'c':
            uval = va_arg(args, uint32_t);
            if (pos < limit) buf[pos++] = (char)uval;
            break;
        default:
            if (pos < limit) buf[pos++] = format[i];
            break;
        }
    }
    buf[pos < bufsize ? pos : bufsize - 1] = '\0';
}

void vfprintf(fs_node_t *file, const char *format, va_list args)
{
    char buf[BUFFER_SIZE];
    vsnprintf(buf, BUFFER_SIZE, format, args);
    write_fs(file, strlen(buf), 1, (uint8_t *)buf);
}

void printf(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stdout_node, format, args);
    va_end(args);
}