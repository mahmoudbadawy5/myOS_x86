#include <vga.h>
#include <stdio.h>
#include <mem/phys_mem.h>
#include <stdarg.h>

char *buf = 0;

void print_uint(unsigned int val, int base, int size)
{
    if (!buf)
        buf = (char *)alloc_blocks(1);
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
    buf[cursz] = '\0';
    // Reverse string
    for (int i = 0; i < cursz / 2; i++)
    {
        char tmp = buf[i];
        buf[i] = buf[cursz - i - 1];
        buf[cursz - i - 1] = tmp;
    }
    puts(buf);
}

void print_int(int val, int base, int size)
{

    buf = (char *)0x32674;
    if (val < 0)
    {
        buf[0] = '-';
        buf++;
        val *= -1;
    }
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
    buf[cursz] = '\0';
    // Reverse string
    for (int i = 0; i < cursz / 2; i++)
    {
        char tmp = buf[i];
        buf[i] = buf[cursz - i - 1];
        buf[cursz - i - 1] = tmp;
    }
    puts(buf);
}

void panic(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    puts("\x1b\x40");
    vprintf(format, args);
    va_end(args);
    __asm__("cli; hlt;");
}

void vprintf(const char *format, va_list args)
{
    /*char **arg = (char **)&format;
    arg++;*/
    for (int i = 0; format[i]; i++)
    {
        if (format[i] != '%')
        {
            putch(format[i]);
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
            putch('%');
            break;
        case 'd':
            ival = va_arg(args, int32_t);
            print_int(ival, 10, sz);
            break;
        case 'x':
            ival = va_arg(args, int32_t);
            print_int(ival, 16, sz);
            break;
        case 'u':
            if (format[i + 1] == 'x')
                base = 16, i++;
            else if (format[i + 1] == 'd')
                base = 10, i++;
            else
                base = 10;
            uval = va_arg(args, uint32_t);
            print_uint(uval, base, sz);
            break;
        case 's':
            sval = va_arg(args, char *);
            puts(sval);
            break;
        case 'c':
            uval = va_arg(args, uint32_t);
            putch((char)uval);
        default:
            putch(format[i]);
        }
    }
}

void printf(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}