#pragma once

#include <types.h>
#include <process.h>

#define VMA_READ   0x1
#define VMA_WRITE  0x2
#define VMA_EXEC   0x4
#define VMA_STACK  0x8
#define VMA_HEAP   0x10
#define VMA_FILE   0x20
#define VMA_GUARD  0x40

void alloc_mem_area(pcb_t* process, uint32_t start, uint32_t size, uint32_t flags);
