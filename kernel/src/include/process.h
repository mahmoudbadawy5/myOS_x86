#ifndef PROCESS_H
#define PROCESS_H

#include <types.h>

#define MAX_PROCESSES 10

typedef enum {
    PROCESS_STATE_READY,
    PROCESS_STATE_RUNNING,
    PROCESS_STATE_BLOCKED,
    PROCESS_STATE_TERMINATED
} process_state_t;

typedef struct {
    u32 eax, ebx, ecx, edx, esi, edi, ebp, esp, eip, eflags, cr3;
} registers_t;

typedef struct {
    u32 pid;
    process_state_t state;
    registers_t regs;
    u8* stack;
} pcb_t;

void init_multitasking();
void create_process(void (*entry)());

#endif
