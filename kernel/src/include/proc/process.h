#pragma once

#include <types.h>
#include <isr.h>
#include <stdio.h>

#define MAX_PROCESSES 10
#define USER_CODE_BASE  0x08048000
#define USER_STACK_TOP  0xA0000000
#define USER_STACK_PAGES 4
#define KERNEL_STACK_SIZE (2 * 4096)

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
    FILE* files_open[MAX_FILES];

    uint32_t children_id[MAX_PROCESSES];
    uint32_t parent_id;

    int signal_pending;     /* Non-zero = pending signal (e.g. SIGINT=2) */
    uint32_t num_children;  /* Number of live children */
    char proc_name[20];
    uint32_t kernel_stack_alloc; /* Base of malloc'd kernel stack (for free) */
    uint32_t kernel_stack_bottom; /* Lowest mapped page of kernel stack */
    char cwd[256];               /* Current working directory */
} pcb_t;

void init_multitasking(void);
void create_process(const char *app_name, uint32_t parent_pid, int argc, const char **argv);
int load_program(pcb_t *proc, const char *path, int argc, const char **argv);
void schedule(struct regs *r);

uint32_t find_terminated_child(uint32_t parent_pid);
int has_live_children(uint32_t parent_pid);
void unblock_parent(uint32_t child_pid);
pcb_t *get_process_by_pid(uint32_t pid);
void remove_child_from_parent(pcb_t *parent, uint32_t child_pid);
void kill_children_of(uint32_t parent_pid);
void process_cleanup_child(pcb_t *child);

extern pcb_t *current_process;
extern pcb_t process_table[];
