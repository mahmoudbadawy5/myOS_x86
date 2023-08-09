#include <vga.h>
#include <stdio.h>
#include <mem/phys_mem.h>

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

void panic(const char *str)
{
    puts(str);
    __asm__("cli; hlt;");
}

void printf(const char *format, ...)
{
    char **arg = (char **)&format;
    arg++;
    for (int i = 0; format[i]; i++)
    {
        if (format[i] != '%')
        {
            putch(format[i]);
            continue;
        }
        i++;
        int sz = 1, base = 10;
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
            print_int(*(int *)arg++, 10, sz);
            break;
        case 'x':
            print_int(*(int *)arg++, 16, sz);
            break;
        case 'u':
            if (format[i + 1] == 'x')
                base = 16, i++;
            else if (format[i + 1] == 'd')
                base = 10, i++;
            else
                base = 10;
            print_uint(*(int *)arg++, base, sz);
            break;
        case 's':
            puts(*arg++);
            break;
        case 'c':
            putch(*(char *)arg++);
        default:
            putch(format[i]);
        }
    }
}