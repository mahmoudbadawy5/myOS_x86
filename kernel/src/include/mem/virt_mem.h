#pragma once

#define PAGE_PRESENT 0x00000001
#define PAGE_RW 0x00000002
#define PAGE_USER 0x00000004
#define PAGE_ADDR 0xFFFFF000

#define USER_SPACE_START    0x40000000  // 1GB
#define USER_SPACE_END      0xBFFFFFFF  // 3GB  
#define USER_STACK_TOP      0xBFFFF000  // User stack top
#define USER_STACK_SIZE     0x2000      // 8KB stack

uint32_t *get_page(uint32_t virt_address);
void *allocate_page(uint32_t *page);
void free_page(uint32_t *page);
void map_address(void *virt_address, void *phys_address);
void unmap_address(void *virt_address);
void init_paging();

// User space helper functions
uint32_t *create_user_space_page();
void switch_page_dir(uint32_t* page_dir);
