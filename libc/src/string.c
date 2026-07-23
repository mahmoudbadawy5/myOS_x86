#include <string.h>
#include <stdlib.h>

void *memcpy(void *dest, const void *src, int count)
{
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    for (int i = 0; i < count; i++)
        d[i] = s[i];
    return dest;
}

void *memmove(void *dest, const void *src, int count)
{
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    if (d < s) {
        for (int i = 0; i < count; i++)
            d[i] = s[i];
    } else {
        for (int i = count - 1; i >= 0; i--)
            d[i] = s[i];
    }
    return dest;
}

void *memset(void *dest, int val, int count)
{
    unsigned char *d = (unsigned char *)dest;
    for (int i = 0; i < count; i++)
        d[i] = (unsigned char)val;
    return dest;
}

int memcmp(const void *s1, const void *s2, int n)
{
    const unsigned char *a = (const unsigned char *)s1;
    const unsigned char *b = (const unsigned char *)s2;
    for (int i = 0; i < n; i++) {
        if (a[i] != b[i])
            return a[i] - b[i];
    }
    return 0;
}

int strlen(const char *str)
{
    int cnt = 0;
    while (str[cnt])
        cnt++;
    return cnt;
}

char *strcpy(char *dest, const char *src)
{
    int i;
    for (i = 0; src[i]; i++)
        dest[i] = src[i];
    dest[i] = '\0';
    return dest;
}

char *strncpy(char *dest, const char *src, int n)
{
    int i;
    for (i = 0; i < n && src[i]; i++)
        dest[i] = src[i];
    for (; i < n; i++)
        dest[i] = '\0';
    return dest;
}

char *strcat(char *dest, const char *src)
{
    int i = strlen(dest);
    int j = 0;
    while (src[j])
        dest[i++] = src[j++];
    dest[i] = '\0';
    return dest;
}

char *strncat(char *dest, const char *src, int n)
{
    int i = strlen(dest);
    int j = 0;
    while (j < n && src[j])
        dest[i++] = src[j++];
    dest[i] = '\0';
    return dest;
}

int strcmp(const char *s1, const char *s2)
{
    while (*s1 && *s1 == *s2) {
        s1++;
        s2++;
    }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

int strncmp(const char *s1, const char *s2, int n)
{
    for (int i = 0; i < n; i++) {
        if (s1[i] != s2[i])
            return (unsigned char)s1[i] - (unsigned char)s2[i];
        if (s1[i] == '\0')
            return 0;
    }
    return 0;
}

int strcasecmp(const char *s1, const char *s2)
{
    while (*s1 && *s2) {
        char a = *s1, b = *s2;
        if (a >= 'a' && a <= 'z') a -= 32;
        if (b >= 'a' && b <= 'z') b -= 32;
        if (a != b) return (unsigned char)a - (unsigned char)b;
        s1++;
        s2++;
    }
    return (unsigned char)*s1 - (unsigned char)*s2;
}

int strncasecmp(const char *s1, const char *s2, int n)
{
    for (int i = 0; i < n; i++) {
        char a = s1[i], b = s2[i];
        if (a >= 'a' && a <= 'z') a -= 32;
        if (b >= 'a' && b <= 'z') b -= 32;
        if (a != b) return (unsigned char)a - (unsigned char)b;
        if (a == '\0') return 0;
    }
    return 0;
}

char *strchr(const char *s, int c)
{
    while (*s) {
        if (*s == (char)c)
            return (char *)s;
        s++;
    }
    return (c == '\0') ? (char *)s : (char *)0;
}

char *strrchr(const char *s, int c)
{
    const char *last = (char *)0;
    while (*s) {
        if (*s == (char)c)
            last = s;
        s++;
    }
    if (c == '\0')
        return (char *)s;
    return (char *)last;
}

char *strstr(const char *haystack, const char *needle)
{
    if (!*needle)
        return (char *)haystack;
    int nlen = strlen(needle);
    while (*haystack) {
        if (strncmp(haystack, needle, nlen) == 0)
            return (char *)haystack;
        haystack++;
    }
    return (char *)0;
}

char *strdup(const char *s)
{
    int len = strlen(s) + 1;
    char *dup = malloc(len);
    if (dup)
        memcpy(dup, s, len);
    return dup;
}

static char *strtok_next = (char *)0;

char *strtok(char *str, const char *delim)
{
    if (str)
        strtok_next = str;
    if (!strtok_next)
        return (char *)0;

    char *start = strtok_next;

    while (*strtok_next) {
        const char *d = delim;
        while (*d) {
            if (*strtok_next == *d) {
                *strtok_next = '\0';
                strtok_next++;
                if (start != strtok_next - 1)
                    return start;
                start = strtok_next;
                break;
            }
            d++;
        }
        if (!*d)
            strtok_next++;
    }

    strtok_next = (char *)0;
    if (start && *start)
        return start;
    return (char *)0;
}
