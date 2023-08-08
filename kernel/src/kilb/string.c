#include <string.h>

unsigned char *memcpy(unsigned char *dest, const unsigned char *src, int count)
{
    int i = 0;
    while (i < count)
    {
        dest[i] = src[i];
        i++;
    }
    return dest;
}
unsigned char *memset(unsigned char *dest, unsigned char val, int count)
{
    int i = 0;
    while (i < count)
        dest[i++] = val;
    return dest;
}
unsigned short *memsetw(unsigned short *dest, unsigned short val, int count)
{
    int i = 0;
    while (i < count)
        dest[i++] = val;
    return dest;
}

int strlen(const char *str)
{
    int cnt = 0;
    while (str[cnt])
        cnt++;
    return cnt;
}
