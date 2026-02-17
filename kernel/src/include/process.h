#pragma once

#include <types.h>

#define MAX_PROCESSES 10
#define USER_CODE_BASE  0x40000000
#define USER_STACK_TOP  0xA0000000
#define USER_STACK_PAGES 4
#define KERNEL_STACK_SIZE (2 * 4096)
#define KERNEL_STACK_BASE 0xC0400000

typedef enum {
    PROCESS_STATE_READY,
    PROCESS_STATE_RUNNING,
    PROCESS_STATE_BLOCKED,
    PROCESS_STATE_TERMINATED
} process_state_t;

typedef struct {
    uint32_t eax, ebx, ecx, edx, esi, edi, ebp, esp, eip, eflags, cr3;
    uint32_t cs, ds, es, fs, gs, ss;
} registers_t;

typedef struct pcb {
    uint32_t pid;
    process_state_t state;
    uint32_t first_run;
    uint32_t kernel_stack_top;
    registers_t regs;
} pcb_t;

void init_multitasking(void);
void create_process(const char *app_name);
void schedule(void);

extern pcb_t *current_process;
