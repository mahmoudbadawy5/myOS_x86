#pragma once

#define PAGE_PRESENT 0x00000001
#define PAGE_RW 0x00000002
#define PAGE_USER 0x00000004

void init_paging();