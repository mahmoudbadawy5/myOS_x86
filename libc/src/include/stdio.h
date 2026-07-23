#ifndef _STDIO_H
#define _STDIO_H

#include <stdarg.h>
#include <stddef.h>

#define EOF (-1)
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#define BUFSIZ 1024
#define MAX_FILES 32

#define FILE_READ  0x01
#define FILE_WRITE 0x02
#define FILE_EOF   0x04
#define FILE_ERROR 0x08

typedef struct _FILE {
    int fd;
    int flags;
    int eof;
    int error;
} FILE;

extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

FILE *fopen(const char *path, const char *mode);
int fclose(FILE *fp);
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *fp);
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *fp);
int fseek(FILE *fp, int offset, int whence);
int ftell(FILE *fp);
int feof(FILE *fp);
int ferror(FILE *fp);
char *fgets(char *s, int size, FILE *fp);
int fgetc(FILE *fp);
int fputs(const char *s, FILE *fp);
int fputc(int c, FILE *fp);
int fprintf(FILE *fp, const char *format, ...);
int printf(const char *format, ...);
int sprintf(char *str, const char *format, ...);
int snprintf(char *str, size_t size, const char *format, ...);
int vsnprintf(char *buf, size_t size, const char *format, va_list args);
int puts(const char *s);
int putchar(int c);
int getchar(void);
void perror(const char *s);

#endif
