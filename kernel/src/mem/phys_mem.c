#include <mem/phys_mem.h>
#include <types.h>
#include <string.h>
#include <stdio.h>
#include <arch.h>

uint32_t *mem_bitmap;

void set_block(int id, int val)
{
    mem_bitmap[id / 32] = (mem_bitmap[id / 32] & ~(1 << (id % 32))) | (val << (id % 32));
}

int test_block(int id)
{
    return mem_bitmap[id / 32] & (1 << (id % 32));
}

void init_phys_mem(const uint32_t start_address)
{
    mem_bitmap = (uint32_t *)(start_address + KERNEL_VIRTUAL_BASE);
    memset((uint8_t *)mem_bitmap, 0, MAX_BLOCK_ENTRIES * sizeof(mem_bitmap[0]));
}

void initialize_memory_region(uint32_t start, uint32_t size)
{
    int start_block = (start + BLOCK_SIZE - 1) / BLOCK_SIZE; // Rounding up start, Partial free blocks are considered used
    int end_block = (start + size) / BLOCK_SIZE;             // Rounding down here

    for (int i = start_block; i < end_block; i++)
    {
        set_block(i, 1);
    }
}

void deinitialize_memory_region(uint32_t start, uint32_t size)
{
    int start_block = (start) / BLOCK_SIZE;                       // Rounding down start, Partial used blocks are considered used
    int end_block = (start + size + BLOCK_SIZE - 1) / BLOCK_SIZE; // Rounding up here

    for (int i = start_block; i < end_block; i++)
    {
        set_block(i, 0);
    }
}

int find_first_free_blocks(uint32_t size)
{
    if (size == 0) return 0;
    uint32_t limit = MAX_BLOCK_ENTRIES * 32;
    if (size > limit) return -1;

    for (uint32_t i = 0; i <= limit - size; i++)
    {
        if (mem_bitmap[i / 32] == 0)
        {
            i = ((i / 32) + 1) * 32 - 1;
            continue;
        }
        bool found = true;
        for (uint32_t j = 0; j < size; j++)
        {
            if (!test_block(i + j))
            {
                found = false;
                i = i + j;
                break;
            }
        }
        if (found)
            return i;
    }
    return -1;
}

uint32_t *alloc_blocks(uint32_t size)
{
    int start_block = find_first_free_blocks(size);
    if (start_block == -1)
        return NULL;
    for (int i = 0; i < size; i++)
        set_block(start_block + i, 0);
    return (void *)(start_block * BLOCK_SIZE);
}

void free_blocks(uint32_t *address, uint32_t size)
{
    uint32_t start_block = (uint32_t)address / BLOCK_SIZE;
    for (int i = 0; i < size; i++)
        set_block(start_block + i, 1);
}