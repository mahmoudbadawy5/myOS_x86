#pragma once

#include <types.h>
#include <isr.h>
#include <stdio.h>

#define MAX_PROCESSES 10
#define USER_CODE_BASE  0x08048000
#define USER_STACK_TOP  0xA0000000
#define USER_STACK_PAGES 4
#define KERNEL_STACK_SIZE (2 * 4096)
#define SIGNAL_TRAMPOLINE_VADDR 0x7FFFE000  /* Fixed user-space address for signal trampoline */

/* Signal infrastructure */
#define NSIG        32
#define SIG_DFL     0   /* Default disposition */
#define SIG_IGN     1   /* Ignore signal */

/* Signal numbers */
#define SIGHUP      1
#define SIGINT      2
#define SIGQUIT     3
#define SIGILL      4
#define SIGTRAP     5
#define SIGABRT     6
#define SIGBUS      7
#define SIGFPE      8
#define SIGKILL     9
#define SIGUSR1    10
#define SIGSEGV    11
#define SIGUSR2    12
#define SIGPIPE    13
#define SIGALRM    14
#define SIGTERM    15
#define SIGSTOP    17  /* Cannot be caught or ignored */
#define SIGCONT    18
#define SIGCHLD    19
#define SIGTSTP    20  /* Ctrl+Z — can be caught */
#define SIGTTIN    21
#define SIGTTOU    22

/* Bitmask helpers */
#define SIG_BIT(sig) (1U << (sig))

typedef enum {
    PROCESS_STATE_NEW,
    PROCESS_STATE_READY,
    PROCESS_STATE_RUNNING,
    PROCESS_STATE_BLOCKED,
    PROCESS_STATE_TERMINATED,
    PROCESS_STATE_STOPPED   /* Suspended by SIGSTOP/SIGTSTP */
} process_state_t;

typedef struct {
    uint32_t eax, ebx, ecx, edx, esi, edi, ebp, esp, eip, eflags, cr3;
    uint32_t cs, ds, es, fs, gs, ss;
} registers_t;

/* Per-process signal state — grouped for clarity */
typedef struct {
    uint32_t pending;                   /* Pending signals bitmask */
    uint32_t disposition[NSIG];         /* SIG_DFL, SIG_IGN, or handler address */
    uint32_t mask;                      /* Blocked signals bitmask */
    uint32_t saved_mask;                /* Saved mask during signal handler execution */
    uint32_t stopped_by;                /* Signal that stopped us (0 = not stopped) */
    uint32_t in_handler;                /* 1 = currently inside signal handler */
    struct regs frame;                  /* Saved user context for sigreturn */
} signal_state_t;

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

    signal_state_t sig;                 /* All signal-related state */

    uint32_t pgid;                      /* Process group ID */
    uint32_t num_children;              /* Number of live children */
    char proc_name[20];
    uint32_t kernel_stack_alloc;        /* Base of malloc'd kernel stack (for free) */
    uint32_t kernel_stack_bottom;       /* Lowest mapped page of kernel stack */
    char cwd[256];                      /* Current working directory */
} pcb_t;

/* Foreground process group — keyboard sends signals here */
extern uint32_t foreground_pgid;

void init_multitasking(void);
void create_process(const char *app_name, uint32_t parent_pid, int argc, const char **argv);
int load_program(pcb_t *proc, const char *path, int argc, const char **argv);
void schedule(struct regs *r);

uint32_t find_terminated_child(uint32_t parent_pid);
uint32_t find_stopped_child(uint32_t parent_pid);
int has_live_children(uint32_t parent_pid);
void unblock_parent(uint32_t child_pid, int cleanup);
pcb_t *get_process_by_pid(uint32_t pid);
void remove_child_from_parent(pcb_t *parent, uint32_t child_pid);
void kill_children_of(uint32_t parent_pid);
void process_cleanup_child(pcb_t *child);
pcb_t *fork_process(pcb_t *parent, struct regs *regs);

void init_signal_trampoline(void);
void map_signal_trampoline(uint32_t *page_dir);

extern pcb_t *current_process;
extern pcb_t process_table[];
