#pragma once

#define PAGE_PRESENT 0x00000001
#define PAGE_RW 0x00000002
#define PAGE_USER 0x00000004
#define PAGE_ADDR 0xFFFFF000

uint32_t *get_page(uint32_t virt_address);
void *allocate_page(uint32_t *page);
void free_page(uint32_t *page);
void map_address(void *virt_address, void *phys_address);
void unmap_address(void *virt_address);
void init_paging();