#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <syscalls.h>

static FILE _stdin  = { .fd = 0, .flags = FILE_READ,  .eof = 0, .error = 0 };
static FILE _stdout = { .fd = 1, .flags = FILE_WRITE, .eof = 0, .error = 0 };
static FILE _stderr = { .fd = 2, .flags = FILE_WRITE, .eof = 0, .error = 0 };

FILE *stdin  = &_stdin;
FILE *stdout = &_stdout;
FILE *stderr = &_stderr;

static void print_uint(char *buf, unsigned int *pos, unsigned int limit, unsigned int val, int base, int min_width)
{
    char tmp[12];
    int len = 0;
    if (val == 0) {
        tmp[len++] = '0';
    } else {
        while (val) {
            char d = val % base;
            tmp[len++] = d <= 9 ? '0' + d : 'A' + d - 10;
            val /= base;
        }
    }
    int zeros = min_width - len;
    if (zeros < 0) zeros = 0;
    while (zeros > 0 && *pos < limit - 1) {
        buf[(*pos)++] = '0';
        zeros--;
    }
    for (int i = len - 1; i >= 0 && *pos < limit - 1; i--)
        buf[(*pos)++] = tmp[i];
}

static void print_int_val(char *buf, unsigned int *pos, unsigned int limit, int val, int base, int min_width)
{
    if (val < 0 && *pos < limit - 1) {
        buf[(*pos)++] = '-';
        val = -val;
    }
    print_uint(buf, pos, limit, (unsigned int)val, base, min_width);
}

void vsnprintf(char *buf, int bufsize, const char *format, va_list args)
{
    if (bufsize <= 0) return;
    unsigned int pos = 0;
    unsigned int limit = (unsigned int)bufsize - 1;

    for (int i = 0; format[i]; i++) {
        if (format[i] != '%') {
            if (pos < limit) buf[pos++] = format[i];
            continue;
        }
        i++;
        int min_width = 0;
        if (format[i] == '0') {
            i++;
            while (format[i] >= '0' && format[i] <= '9') {
                min_width = min_width * 10 + format[i] - '0';
                i++;
            }
        }
        switch (format[i]) {
        case '%':
            if (pos < limit) buf[pos++] = '%';
            break;
        case 'd': case 'i':
            print_int_val(buf, &pos, limit, va_arg(args, int), 10, min_width);
            break;
        case 'u':
            print_uint(buf, &pos, limit, va_arg(args, unsigned int), 10, min_width);
            break;
        case 'x':
            print_uint(buf, &pos, limit, va_arg(args, unsigned int), 16, min_width);
            break;
        case 'o':
            print_uint(buf, &pos, limit, va_arg(args, unsigned int), 8, min_width);
            break;
        case 's': {
            char *s = va_arg(args, char *);
            if (!s) s = "(null)";
            while (*s && pos < limit)
                buf[pos++] = *s++;
            break;
        }
        case 'c':
            if (pos < limit) buf[pos++] = (char)va_arg(args, int);
            break;
        case 'p': {
            unsigned int p = (unsigned int)(unsigned long)va_arg(args, void *);
            if (pos < limit) buf[pos++] = '0';
            if (pos < limit) buf[pos++] = 'x';
            print_uint(buf, &pos, limit, p, 16, 8);
            break;
        }
        case '\0':
            goto done;
        default:
            if (pos < limit) buf[pos++] = format[i];
            break;
        }
    }
done:
    buf[pos < (unsigned int)bufsize ? pos : (unsigned int)bufsize - 1] = '\0';
}

int printf(const char *format, ...)
{
    char buf[BUFSIZ];
    va_list args;
    va_start(args, format);
    vsnprintf(buf, BUFSIZ, format, args);
    va_end(args);
    return sys_write(buf, strlen(buf));
}

int fprintf(FILE *fp, const char *format, ...)
{
    char buf[BUFSIZ];
    va_list args;
    va_start(args, format);
    vsnprintf(buf, BUFSIZ, format, args);
    va_end(args);
    int fd = fp ? fp->fd : 1;
    return sys_write_fd(fd, buf, strlen(buf));
}

int sprintf(char *str, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vsnprintf(str, 0x7FFFFFFF, format, args);
    va_end(args);
    return strlen(str);
}

int snprintf(char *str, int size, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vsnprintf(str, size, format, args);
    va_end(args);
    return strlen(str);
}

int puts(const char *s)
{
    int len = strlen(s);
    sys_write(s, len);
    sys_write("\n", 1);
    return len + 1;
}

int putchar(int c)
{
    char ch = (char)c;
    sys_write(&ch, 1);
    return c;
}

int getchar(void)
{
    char c;
    sys_read(&c, 1);
    return (int)(unsigned char)c;
}

/* File operations */

FILE *fopen(const char *path, const char *mode)
{
    int fd = sys_open(path, mode);
    if (fd < 0)
        return (FILE *)0;

    FILE *fp = (FILE *)malloc(sizeof(FILE));
    if (!fp) {
        /* Can't allocate FILE; fd is leaked but we can't close without close syscall */
        return (FILE *)0;
    }
    fp->fd = fd;
    fp->flags = 0;
    fp->eof = 0;
    fp->error = 0;

    if (mode) {
        if (mode[0] == 'r')
            fp->flags = FILE_READ;
        else if (mode[0] == 'w')
            fp->flags = FILE_WRITE;
        else if (mode[0] == 'a')
            fp->flags = FILE_WRITE;
    }
    return fp;
}

int fclose(FILE *fp)
{
    if (!fp) return EOF;
    /* No close syscall yet — just free the struct */
    free(fp);
    return 0;
}

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *fp)
{
    if (!fp || fp->fd < 0) return 0;
    int total = size * nmemb;
    int n = sys_read_fd(fp->fd, ptr, total);
    if (n <= 0) {
        fp->eof = 1;
        return 0;
    }
    return n / size;
}

size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *fp)
{
    if (!fp || fp->fd < 0) return 0;
    int total = size * nmemb;
    int n = sys_write_fd(fp->fd, (const char *)ptr, total);
    if (n < 0) {
        fp->error = 1;
        return 0;
    }
    return n / size;
}

int fseek(FILE *fp, int offset, int whence)
{
    if (!fp || fp->fd < 0) return -1;
    int ret = sys_lseek(fp->fd, offset, whence);
    if (ret < 0) {
        fp->error = 1;
        return -1;
    }
    fp->eof = 0;
    return 0;
}

int ftell(FILE *fp)
{
    if (!fp || fp->fd < 0) return -1;
    return sys_lseek(fp->fd, 0, SEEK_CUR);
}

int feof(FILE *fp)
{
    return fp ? fp->eof : 1;
}

int ferror(FILE *fp)
{
    return fp ? fp->error : 1;
}

char *fgets(char *s, int size, FILE *fp)
{
    if (!fp || size <= 0) return (char *)0;
    int i = 0;
    while (i < size - 1) {
        char c;
        int n = sys_read_fd(fp->fd, &c, 1);
        if (n <= 0) {
            fp->eof = 1;
            break;
        }
        s[i++] = c;
        if (c == '\n') break;
    }
    s[i] = '\0';
    return (i > 0) ? s : (char *)0;
}

int fgetc(FILE *fp)
{
    if (!fp) return EOF;
    char c;
    int n = sys_read_fd(fp->fd, &c, 1);
    if (n <= 0) {
        fp->eof = 1;
        return EOF;
    }
    return (int)(unsigned char)c;
}

int fputs(const char *s, FILE *fp)
{
    if (!fp) return EOF;
    return sys_write_fd(fp->fd, s, strlen(s));
}

int fputc(int c, FILE *fp)
{
    char ch = (char)c;
    if (!fp) return EOF;
    return sys_write_fd(fp->fd, &ch, 1);
}

void perror(const char *s)
{
    if (s && *s)
        fprintf(stderr, "%s: ", s);
    fprintf(stderr, "error\n");
}
