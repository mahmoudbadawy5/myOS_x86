#include <types.h>
#include <mem/phys_mem.h>
#include <mem/virt_mem.h>

uint32_t *cur_page_dir = 0;
uint32_t *kernel_page_dir = 0;
uint32_t *kernel_page_table = 0;

void set_page_dir(uint32_t *page_dir)
{
    cur_page_dir = page_dir;
    __asm__ __volatile__("movl %%EAX, %%CR3"
                         :
                         : "a"(cur_page_dir));
}

void enable_paging()
{
    __asm__ __volatile__("movl %CR0, %EAX;"
                         "orl $0x80000001, %EAX;"
                         "movl %EAX, %CR0;");
}

void init_paging()
{
    /*
     * Initializes paging with the first PageTable (first 4MB) with identity paging
     */
    kernel_page_dir = alloc_blocks(1);
    kernel_page_table = alloc_blocks(1);
    for (int i = 0; i < 1024; i++)
        kernel_page_dir[i] = PAGE_RW;                                            // Setting all pages to R/W and not present
    kernel_page_dir[0] = ((uint32_t)kernel_page_table) | PAGE_PRESENT | PAGE_RW; // R/W and enabled
    for (int i = 0; i < 1024; i++)
        kernel_page_table[i] = (i << 12) | PAGE_PRESENT | PAGE_RW;
    set_page_dir(kernel_page_dir);
    enable_paging();
}