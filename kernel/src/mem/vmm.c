#include <assert.h>
#include <process.h>
#include <mem/vmm.h>
#include <mem/malloc.h>
#include <mem/phys_mem.h>
#include <mem/virt_mem.h>

void alloc_mem_area(pcb_t* process, uint32_t start, uint32_t size, uint32_t flags) {
    // Assumes you are in the correct page_dir
    start &= ~(BLOCK_SIZE - 1);
    uint32_t end = (start + size + BLOCK_SIZE - 1) & ~(BLOCK_SIZE - 1);
    vma_t *cur = process->memory_regions;
    vma_t *last = NULL;
    while(cur) {
        ASSERT(end <= cur->start || cur->end <= start, "Memory areas overlapping [%d, %d] [%d, %d]", start, end, last->start, last->end);
        last = cur;
        cur = cur->next;
    }
    vma_t *new_area = malloc(sizeof(vma_t));
    new_area->start=start;
    new_area->end=end;
    new_area->flags=flags;
    new_area->next=NULL;
    for (uint32_t i = 0; i < (size + BLOCK_SIZE - 1) / BLOCK_SIZE; i++) {
        void *virt = (void *)(start + i * BLOCK_SIZE);
        void *phys = alloc_blocks(1);
        ASSERT(phys != NULL, "Coulnd't assign memory");
        map_address_user(virt, phys);
    }
    if(last) last->next = new_area;
    else process->memory_regions=new_area;
}
