#include <mem/malloc.h>
#include <mem/phys_mem.h>
#include <mem/virt_mem.h>
#include <arch.h>

malloc_header_t *malloc_head = 0;

// First 4MB of virtual memory is already reserved for kernel/BIOS
// Starting mallocing after 4MB;

uint32_t malloc_virt_address = 4 * 1024 * 1024 + KERNEL_VIRTUAL_BASE;
uint32_t malloc_reserved_blocks = 0;

void init_malloc()
{
    malloc_reserved_blocks = (MALLOC_INITIAL_SIZE + sizeof(malloc_header_t) + BLOCK_SIZE - 1) / BLOCK_SIZE; // Reserving enough memory for initial size + header size rounded up
    uint32_t actual_reserved = malloc_reserved_blocks * BLOCK_SIZE - sizeof(malloc_header_t);
    for (int i = 0; i < malloc_reserved_blocks; i++)
    {
        uint32_t *phys_address = alloc_blocks(1);
        map_address((void *)malloc_virt_address + i * BLOCK_SIZE, (void *)phys_address);
    }
    malloc_head = (malloc_header_t *)malloc_virt_address;
    malloc_head->next = NULL;
    malloc_head->free = true;
    malloc_head->size = actual_reserved;
}

void malloc_split(malloc_header_t *node, int req_size)
{
    if (req_size + sizeof(malloc_header_t) >= node->size)
        return; // No need to split or failed
    malloc_header_t *new_node = (malloc_header_t *)((void *)node + req_size + sizeof(malloc_header_t));
    new_node->next = node->next;
    new_node->free = node->free;
    new_node->size = node->size - req_size - sizeof(malloc_header_t);
    node->size = req_size;
    node->next = new_node;
}

void *malloc(uint32_t size)
{
    malloc_header_t *cur = malloc_head;
    while ((cur->size < size || cur->free == false) && cur->next)
    {
        cur = cur->next;
    }
    if (cur->size >= size && cur->free) // Bingo we found it
    {
        malloc_split(cur, size);
        cur->free = false;
        return (void *)cur + sizeof(malloc_header_t);
    }
    else // Need to extend more :'(
    {
        if (!cur->free)
        {
            // Create a new block
            int needed_blocks = (size + sizeof(malloc_header_t) + BLOCK_SIZE - 1) / BLOCK_SIZE;
            for (int i = malloc_reserved_blocks; i < malloc_reserved_blocks + needed_blocks; i++)
            {
                uint32_t *phys_address = alloc_blocks(1);
                map_address((void *)malloc_virt_address + i * BLOCK_SIZE, (void *)phys_address);
            }
            malloc_header_t *new_node = (malloc_header_t *)((void *)malloc_virt_address + malloc_reserved_blocks * BLOCK_SIZE);
            new_node->size = needed_blocks * BLOCK_SIZE - sizeof(malloc_header_t);
            new_node->free = true;
            new_node->next = NULL;
            cur->next = new_node;
            cur = new_node;
            malloc_reserved_blocks += needed_blocks;
        }
        else
        {
            // Extend current block
            int needed_blocks = (size - cur->free + BLOCK_SIZE - 1) / BLOCK_SIZE;
            for (int i = malloc_reserved_blocks; i < malloc_reserved_blocks + needed_blocks; i++)
            {
                uint32_t *phys_address = alloc_blocks(1);
                map_address((void *)malloc_virt_address + i * BLOCK_SIZE, (void *)phys_address);
            }
            malloc_reserved_blocks += needed_blocks;
            cur->size += needed_blocks * BLOCK_SIZE;
        }
        malloc_split(cur, size);
        cur->free = false;
        return (void *)cur + sizeof(malloc_header_t);
    }
}

void free(void *address) // Needs a valid address that was created by malloc
{
    malloc_header_t *cur = malloc_head;
    malloc_header_t *prev = NULL;
    while (cur)
    {
        if ((void *)cur + sizeof(malloc_header_t) == address)
        {
            cur->free = true;
            if (prev && prev->free) // Merge with prev
            {
                prev->size += cur->size + sizeof(malloc_header_t);
                prev->next = cur->next;
                cur = prev;
            }
            if (cur->next && cur->next->free) // Merge with next
            {
                cur->size += cur->next->size + sizeof(malloc_header_t);
                cur->next = cur->next->next;
            }
            return;
        }
        prev = cur;
        cur = cur->next;
    }
}