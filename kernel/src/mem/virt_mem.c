#include <types.h>
#include <mem/phys_mem.h>
#include <mem/virt_mem.h>
#include <string.h>
#include <arch.h>
#include <stdio.h>

uint32_t *cur_page_dir = 0;
uint32_t *kernel_page_dir = 0;
uint32_t *kernel_page_table = 0;

void set_page_dir(uint32_t *page_dir)
{
    cur_page_dir = (uint32_t *)((uint32_t)page_dir + KERNEL_VIRTUAL_BASE);
    __asm__ __volatile__("movl %%EAX, %%CR3"
                         :
                         : "a"(page_dir));
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
        memset((uint8_t *)((uint32_t)table + KERNEL_VIRTUAL_BASE), 0, sizeof(*table) * 1024);
        *table_entry = (uint32_t)table;
        // printf("Table enrty created: %ux\n", (uint32_t)table_entry);
        *table_entry |= (PAGE_PRESENT | PAGE_RW);
    }
    uint32_t *table = (uint32_t *)((*table_entry + KERNEL_VIRTUAL_BASE) & PAGE_ADDR);
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
    void *address = (void *)((*page - KERNEL_VIRTUAL_BASE) & PAGE_ADDR);
    if (address)
        free_blocks(address, 1);
    *page &= ~PAGE_PRESENT;
}

void map_address(void *virt_address, void *phys_address)
{
    uint32_t *page = get_page((uint32_t)virt_address);
    *page &= ~PAGE_ADDR;                           // Clear old address
    *page |= ((uint32_t)phys_address & PAGE_ADDR); // Set new address
    *page |= PAGE_PRESENT | PAGE_RW;                // Set page present, kernel only
}

void map_address_user(void *virt_address, void *phys_address)
{
    uint32_t *page = get_page((uint32_t)virt_address);
    *page &= ~PAGE_ADDR;
    *page |= ((uint32_t)phys_address & PAGE_ADDR);
    *page |= PAGE_PRESENT | PAGE_RW | PAGE_USER;    // User-accessible (ring 3)
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
    kernel_page_dir = (uint32_t *)((uint32_t)alloc_blocks(1) + KERNEL_VIRTUAL_BASE);
    kernel_page_table = (uint32_t *)((uint32_t)alloc_blocks(1) + KERNEL_VIRTUAL_BASE);

    for (int i = 0; i < 1024; i++)
        kernel_page_dir[i] = PAGE_RW;                                                                                   // Setting all pages to R/W and not present
    kernel_page_dir[KERNEL_PAGE_NUMBER] = ((uint32_t)kernel_page_table - KERNEL_VIRTUAL_BASE) | PAGE_PRESENT | PAGE_RW; // R/W and enabled
    for (int i = 0; i < 1024; i++)
        kernel_page_table[i] = (i << 12) | PAGE_PRESENT | PAGE_RW;

    for (int i = 0; i < 5; i++)
        print_hex(kernel_page_table[i]);
    set_page_dir((uint32_t *)((uint32_t)kernel_page_dir - KERNEL_VIRTUAL_BASE));
    enable_paging();
}

uint32_t *vmm_get_directory(void)
{
    return (uint32_t *)((uint32_t)cur_page_dir - KERNEL_VIRTUAL_BASE);
}

uint32_t *vmm_clone_directory(void)
{
    uint32_t *new_dir = (uint32_t *)((uint32_t)alloc_blocks(1) + KERNEL_VIRTUAL_BASE);
    memset((unsigned char *)new_dir, 0, 1024 * sizeof(uint32_t));

    for (uint32_t i = 0; i < 1024; i++) {
        if (!(cur_page_dir[i] & PAGE_PRESENT))
            continue;
        uint32_t *new_table = (uint32_t *)((uint32_t)alloc_blocks(1) + KERNEL_VIRTUAL_BASE);
        uint32_t *old_table = (uint32_t *)((cur_page_dir[i] & PAGE_ADDR) + KERNEL_VIRTUAL_BASE);
        if (i >= KERNEL_PAGE_NUMBER) {
            memcpy((unsigned char *)new_table, (const unsigned char *)old_table, 1024 * sizeof(uint32_t));
        } else {
            memset((unsigned char *)new_table, 0, 1024 * sizeof(uint32_t));
        }
        new_dir[i] = ((uint32_t)new_table - KERNEL_VIRTUAL_BASE) | (cur_page_dir[i] & 0xFFF);
    }

    return (uint32_t *)((uint32_t)new_dir - KERNEL_VIRTUAL_BASE);
}

void switch_to_kernel_page_dir(void)
{
    if (kernel_page_dir)
        set_page_dir((uint32_t *)((uint32_t)kernel_page_dir - KERNEL_VIRTUAL_BASE));
}