#pragma once

#include <types.h>

typedef struct cpu_state {
    uint32_t eip, esp, ebp;
    uint32_t eax, ebx, ecx, edx;
    uint32_t esi, edi;
    uint32_t eflags;
} cpu_state_t;

typedef struct task {
    cpu_state_t state;
    uint32_t* page_directory;  // for per-process paging
    struct task* next;
} task_t;

void init_tasking();
void schedule();
