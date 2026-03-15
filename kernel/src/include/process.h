#pragma once

#include <types.h>
#include <isr.h>

#define MAX_PROCESSES 10
#define USER_CODE_BASE  0x40000000
#define USER_STACK_TOP  0xA0000000
#define USER_STACK_PAGES 4
#define KERNEL_STACK_SIZE (2 * 4096)
#define KERNEL_STACK_BASE 0xC0400000

typedef enum {
    PROCESS_STATE_NEW,
    PROCESS_STATE_READY,
    PROCESS_STATE_RUNNING,
    PROCESS_STATE_BLOCKED,
    PROCESS_STATE_TERMINATED
} process_state_t;

typedef struct {
    uint32_t eax, ebx, ecx, edx, esi, edi, ebp, esp, eip, eflags, cr3;
    uint32_t cs, ds, es, fs, gs, ss;
} registers_t;

typedef struct vma {
    uint32_t start;        // virtual address start (page-aligned)
    uint32_t end;          // virtual address end   (page-aligned, exclusive)
    uint32_t flags;        // VMA_READ | VMA_WRITE | VMA_EXEC | VMA_USER
    struct vma *next;
} vma_t;

typedef struct pcb {
    uint32_t pid;
    process_state_t state;
    uint32_t kernel_stack_top;
    registers_t regs;
    vma_t *memory_regions;
} pcb_t;

void init_multitasking(void);
void create_process(const char *app_name);
void schedule(struct regs *r);

extern pcb_t *current_process;
