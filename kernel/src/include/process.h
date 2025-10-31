#pragma once

#include <types.h>

#define MAX_PROCESSES 10


typedef enum {
    PROCESS_STATE_READY,
    PROCESS_STATE_RUNNING,
    PROCESS_STATE_BLOCKED,
    PROCESS_STATE_TERMINATED
} process_state_t;

typedef struct {
    uint32_t eax, ebx, ecx, edx, esi, edi, ebp, esp, eip, eflags, cr3;
} registers_t;

typedef struct {
    uint32_t pid;
    process_state_t state;
    registers_t regs;
} pcb_t;

void init_multitasking();
void create_process(char* app_name);
void schedule();
