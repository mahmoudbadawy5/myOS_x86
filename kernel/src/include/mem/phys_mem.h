#pragma once
#include <types.h>

#define BLOCK_SIZE 4096
#define MAX_BLOCK_ENTRIES 4096 // max accessible memory = 4096*4096*32 (512 MB)

void init_phys_mem(const uint32_t start_address);
void initialize_memory_region(uint32_t start, uint32_t size);
void deinitialize_memory_region(uint32_t start, uint32_t size);
int find_first_free_blocks(uint32_t size);
uint32_t *alloc_blocks(uint32_t size);
void free_blocks(uint32_t *address, uint32_t size);
