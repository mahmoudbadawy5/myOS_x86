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

int strcmp(const char *str1, const char *str2)
{
    int i = 0;
    while (str1[i] && str2[i])
    {
        if (str1[i] != str2[i])
        {
            return str1[i] < str2[i] ? -(i + 1) : i + 1;
        }
        i++;
    }
    if (str1[i])
        return -(i + 1);
    if (str2[i])
        return i + 1;
    return 0;
}

char *strcpy(char *destination, const char *source)
{
    int i;
    for (i = 0; source[i]; i++)
        destination[i] = source[i];
    destination[i] = '\0';
    return destination;
}