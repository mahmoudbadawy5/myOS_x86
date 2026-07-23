#include <stdlib.h>
#include <string.h>
#include <syscalls.h>

typedef struct _malloc_header {
    size_t size;
    int free;
    struct _malloc_header *next;
} malloc_header_t;

static malloc_header_t *heap_base = (void *)0;
static malloc_header_t *last_alloc = (void *)0;

static malloc_header_t *find_free(size_t size)
{
    malloc_header_t *cur = heap_base;
    while (cur) {
        if (cur->free && cur->size >= size)
            return cur;
        cur = cur->next;
    }
    return (void *)0;
}

void *malloc(size_t size)
{
    if (size == 0) return (void *)0;

    malloc_header_t *header = find_free(size);
    if (header) {
        header->free = 0;
        return (void *)((char *)header + sizeof(malloc_header_t));
    }

    size_t total = size + sizeof(malloc_header_t);
    header = (malloc_header_t *)sys_sbrk(total);
    if ((void *)header == (void *)0 || (void *)header == (void *)-1)
        return (void *)0;

    header->size = size;
    header->free = 0;
    header->next = (void *)0;

    if (last_alloc)
        last_alloc->next = header;
    else
        heap_base = header;
    last_alloc = header;

    return (void *)((char *)header + sizeof(malloc_header_t));
}

void free(void *ptr)
{
    if (!ptr) return;

    malloc_header_t *header = (malloc_header_t *)((char *)ptr - sizeof(malloc_header_t));
    header->free = 1;
}

void *realloc(void *ptr, size_t size)
{
    if (!ptr) return malloc(size);
    if (size == 0) { free(ptr); return (void *)0; }

    malloc_header_t *header = (malloc_header_t *)((char *)ptr - sizeof(malloc_header_t));
    if (header->size >= size) return ptr;

    void *new_ptr = malloc(size);
    if (new_ptr) {
        memcpy(new_ptr, ptr, header->size);
        free(ptr);
    }
    return new_ptr;
}

int atoi(const char *s)
{
    int result = 0;
    int sign = 1;
    while (*s == ' ' || *s == '\t') s++;
    if (*s == '-') { sign = -1; s++; }
    else if (*s == '+') s++;
    while (*s >= '0' && *s <= '9')
        result = result * 10 + (*s++ - '0');
    return result * sign;
}

long atol(const char *s)
{
    return (long)atoi(s);
}

int abs(int n)
{
    return n < 0 ? -n : n;
}

long labs(long n)
{
    return n < 0 ? -n : n;
}
