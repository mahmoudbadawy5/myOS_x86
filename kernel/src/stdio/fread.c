#include <fs/vfs.h>
#include <stdio.h>

size_t fread(char *buf, size_t size, size_t count, FILE *fp)
{
    if ((fp->flags & FILE_READ) != FILE_READ)
        return 0;
    return read_fs(fp->file, size, count, (uint8_t *)buf);
}