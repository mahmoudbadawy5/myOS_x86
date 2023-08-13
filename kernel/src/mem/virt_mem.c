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

uint32_t *get_page(uint32_t virt_address)
{
    uint32_t pd_entry = (virt_address) >> 22;
    uint32_t pt_entry = ((virt_address) >> 12) & 0x3FF;
    // printf("Getting page for %ux, pd_entry: %ux, pt_enrty: %ud\n", virt_address, pd_entry, pt_entry);
    uint32_t *table_entry = &cur_page_dir[pd_entry];
    // printf("Table enrty was: %ux\n", (*table_entry));
    if ((*table_entry & PAGE_PRESENT) == 0)
    {
        uint32_t *table = (uint32_t *)alloc_blocks(1);
        memset(table, 0, sizeof(*table) * 1024);
        *table_entry = table;
        // printf("Table enrty created: %ux\n", (uint32_t)table_entry);
        *table_entry |= (PAGE_PRESENT | PAGE_RW);
    }
    uint32_t *table = (uint32_t *)(*table_entry & PAGE_ADDR);
    uint32_t *page_entry = &table[pt_entry];
    // printf("Page entry is: %ux\n", (uint32_t)page_entry);
    return page_entry;
}

void *allocate_page(uint32_t *page)
{
    void *phys_addr = alloc_blocks(1);
    if (phys_addr)
    {
        *page |= ((uint32_t)phys_addr & PAGE_ADDR);
        *page |= (PAGE_PRESENT);
    }
    return phys_addr;
}

void free_page(uint32_t *page)
{
    void *address = (void *)((*page) & PAGE_ADDR);
    if (address)
        free_blocks(address, 1);
    *page &= ~PAGE_PRESENT;
}

void map_address(void *virt_address, void *phys_address)
{
    uint32_t *page = get_page((uint32_t)virt_address);
    *page |= ((uint32_t)phys_address & PAGE_ADDR);
    *page |= PAGE_PRESENT;
}

void unmap_address(void *virt_address)
{
    uint32_t *page = get_page((uint32_t)virt_address);
    *page &= ~PAGE_ADDR;
    *page &= ~PAGE_PRESENT;
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