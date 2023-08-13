#pragma once

#include <types.h>

#define MALLOC_INITIAL_SIZE (1024 * 1024)

typedef struct malloc_header
{
    uint32_t size;
    bool free;
    struct malloc_header *next;
} malloc_header_t;

void init_malloc();
void *malloc(uint32_t size);
void free(void *address);
