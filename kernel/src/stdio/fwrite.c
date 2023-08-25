#include <fs/vfs.h>
#include <stdio.h>
#include <string.h>

size_t fwrite(const char *buf, size_t size, size_t count, FILE *fp)
{
    if (!fp)
        return 0;
    if ((fp->flags & FILE_WRITE) != FILE_WRITE)
        return 0;
    return write_fs(fp->file, strlen(buf), 1, (uint8_t *)buf);
}