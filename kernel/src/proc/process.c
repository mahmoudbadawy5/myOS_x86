#include <proc/process.h>
#include <proc/elf.h>
#include <mem/phys_mem.h>
#include <mem/virt_mem.h>
#include <mem/malloc.h>
#include <mem/vmm.h>
#include <string.h>
#include <stdio.h>
#include <fs/vfs.h>
#include <fs/initrd.h>
#include <arch.h>
#include <isr.h>
#include <tss.h>

pcb_t process_table[MAX_PROCESSES];
static uint32_t next_pid = 1;
static uint32_t num_processes = 0, cur_proccess_id = -1;

pcb_t *current_process = NULL;

/* Idle loop — runs when no other process is available.
 * Executes in ring 0 (kernel mode) via IRET with CS=0x08. */
static void idle_loop(void)
{
    for (;;)
        __asm__ __volatile__("hlt");
}

pcb_t *get_process_by_pid(uint32_t pid)
{
    for (uint32_t i = 0; i < num_processes; i++) {
        if (process_table[i].pid == pid)
            return &process_table[i];
    }
    return 0;
}

extern fs_node_t *root_dir;
extern void switch_to_process(pcb_t *next, struct regs* r);

void init_multitasking(void)
{
    memset((unsigned char *)process_table, 0, sizeof(process_table));
    current_process = NULL;
    next_pid = 1;
    num_processes = 0;

    /* Create idle process (PID 0) — runs a HLT loop in ring 0.
     * Always READY; scheduler picks it when no other process is available. */
    pcb_t *idle = &process_table[0];
    idle->pid = 0;
    idle->state = PROCESS_STATE_NEW;
    idle->parent_id = 0;
    idle->signal_pending = 0;
    idle->num_children = 0;
    idle->proc_name[0] = 'i';
    idle->proc_name[1] = 'd';
    idle->proc_name[2] = 'l';
    idle->proc_name[3] = 'e';
    idle->proc_name[4] = '\0';
    idle->cwd[0] = '/';
    idle->cwd[1] = '\0';

    /* Allocate kernel stack for idle */
    uint32_t kstack_virt = (uint32_t)malloc(KERNEL_STACK_SIZE);
    idle->kernel_stack_alloc = kstack_virt;
    idle->kernel_stack_bottom = kstack_virt;
    uint32_t kstack_base = kstack_virt + KERNEL_STACK_SIZE;
    uint32_t iret_frame = (kstack_base - 32) & ~0xF;
    idle->kernel_stack_top = iret_frame;

    /* Build IRET frame: EIP=idle_loop, CS=0x08 (kernel), EFLAGS=IF=1,
     * ESP=kernel stack top, SS=0x10 (kernel data).
     * Idle runs in ring 0 — no user segments needed. */
    uint32_t *frame = (uint32_t *)iret_frame;
    frame[0] = (uint32_t)idle_loop;  /* EIP */
    frame[1] = 0x08;                 /* CS (kernel code) */
    frame[2] = 0x202;                /* EFLAGS (IF=1) */
    frame[3] = iret_frame;           /* ESP (use same stack) */
    frame[4] = 0x10;                 /* SS (kernel data) */

    /* Set up registers with kernel segments */
    idle->regs.cs = 0x08;
    idle->regs.ds = 0x10;
    idle->regs.es = 0x10;
    idle->regs.fs = 0x10;
    idle->regs.gs = 0x10;
    idle->regs.ss = 0x10;
    idle->regs.eflags = 0x202;
    idle->regs.cr3 = (uint32_t)vmm_get_directory();

    num_processes = 1;
}

/* Shared by create_process and exec: clone page dir, load ELF, build argv stack. */
int load_program(pcb_t *proc, const char *path, int argc, const char **argv)
{
    uint32_t old_cr3;
    __asm__ __volatile__("mov %%cr3, %0" : "=r"(old_cr3));
    uint32_t *new_dir = vmm_clone_directory();
    set_page_dir(new_dir);

    memset((unsigned char *)&proc->regs, 0, sizeof(registers_t));
    proc->regs.cr3 = (uint32_t)new_dir;
    proc->regs.eflags = 0x202;
    proc->regs.cs = 0x1B;
    proc->regs.ds = 0x23;
    proc->regs.es = 0x23;
    proc->regs.fs = 0x23;
    proc->regs.gs = 0x23;
    proc->regs.ss = 0x23;

    if (load_elf(proc, path) != 0) {
        set_page_dir((uint32_t *)old_cr3);
        vmm_free_directory(new_dir);
        return -1;
    }

    if (argc > 0 && argv) {
        uint32_t stack_top = USER_STACK_TOP - 16;

        uint32_t total_str_len = 0;
        for (int a = 0; a < argc; a++) {
            const char *s = argv[a];
            uint32_t slen = 0;
            while (s[slen]) slen++;
            total_str_len += slen + 1;
        }

        uint32_t str_area = stack_top - total_str_len;
        str_area &= ~3;

        uint32_t argv_ptrs_size = (argc + 3) * 4;
        uint32_t esp = str_area - argv_ptrs_size;
        esp &= ~3;

        uint32_t *ustack = (uint32_t *)esp;
        ustack[0] = argc;

        uint32_t str_off = str_area;
        for (int a = 0; a < argc; a++) {
            ustack[1 + a] = str_off;
            const char *s = argv[a];
            uint32_t slen = 0;
            while (s[slen]) slen++;
            slen++;
            memcpy((void *)str_off, s, slen);
            str_off += slen;
        }
        ustack[1 + argc] = 0;
        ustack[2 + argc] = 0;

        proc->regs.esp = esp;
        uint32_t *frame = (uint32_t *)proc->kernel_stack_top;
        frame[3] = esp;
    }

    set_page_dir((uint32_t *)old_cr3);
    return 0;
}

void create_process(const char *app_path, uint32_t parent_pid, int argc, const char **argv)
{
    if (num_processes >= MAX_PROCESSES)
        return;

    char kernel_path[128];
    int i = 0;
    while (app_path[i] && i < 127) {
        kernel_path[i] = app_path[i];
        i++;
    }
    kernel_path[i] = '\0';

    uint32_t process_index = num_processes;
    pcb_t *pcb = &process_table[process_index];
    pcb->pid = next_pid++;
    pcb->state = PROCESS_STATE_BLOCKED;
    pcb->parent_id = parent_pid;
    pcb->signal_pending = 0;
    pcb->num_children = 0;
    pcb->files_open[0] = malloc(sizeof(FILE));
    pcb->files_open[0]->file = stdin_node;
    pcb->files_open[0]->flags = FILE_READ;
    
    pcb->files_open[1] = malloc(sizeof(FILE));
    pcb->files_open[1]->file = stdout_node;
    pcb->files_open[1]->flags = FILE_WRITE;
    for(int i=0;i<19;i++) pcb->proc_name[i] = kernel_path[i];
    pcb->proc_name[19]='\0';
    pcb->cwd[0] = '/';
    pcb->cwd[1] = '\0';

    /* Inherit cwd from parent */
    if (parent_pid != 0) {
        for (uint32_t j = 0; j < num_processes; j++) {
            if (process_table[j].pid == parent_pid) {
                int ci = 0;
                while (process_table[j].cwd[ci] && ci < 254) {
                    pcb->cwd[ci] = process_table[j].cwd[ci];
                    ci++;
                }
                pcb->cwd[ci] = '\0';
                break;
            }
        }
    }

    uint32_t kstack_virt = (uint32_t) malloc(KERNEL_STACK_SIZE);
    pcb->kernel_stack_alloc = kstack_virt;
    pcb->kernel_stack_bottom = kstack_virt;

    uint32_t kstack_base = kstack_virt + KERNEL_STACK_SIZE;
    uint32_t iret_frame = (kstack_base - 32) & ~0xF;
    pcb->kernel_stack_top = iret_frame;

    /* Increment parent's child count BEFORE load so it's always accurate */
    if (parent_pid != 0) {
        for (uint32_t j = 0; j < num_processes; j++) {
            if (process_table[j].pid == parent_pid) {
                process_table[j].children_id[process_table[j].num_children] = pcb->pid;
                process_table[j].num_children++;
                break;
            }
        }
    }

    if (load_program(pcb, kernel_path, argc, argv) != 0) {
        /* Undo parent's child count */
        if (parent_pid != 0) {
            for (uint32_t j = 0; j < num_processes; j++) {
                if (process_table[j].pid == parent_pid) {
                    process_table[j].num_children--;
                    break;
                }
            }
        }
        /* Free allocations from create_process */
        free(pcb->files_open[0]);
        free(pcb->files_open[1]);
        free((void *)pcb->kernel_stack_alloc);
        return;
    }

    pcb->state = PROCESS_STATE_NEW;
    num_processes++;
}

void schedule(struct regs *r)
{
    // TODO: make it a linked list so that no zombie processes remain
    if (num_processes == 0)
        return;

    __asm__ __volatile__("cli");

    /* Check for pending signals on the current process */
    if (cur_proccess_id != -1) {
        pcb_t *cur = &process_table[cur_proccess_id];
        if (cur->signal_pending != 0 && cur->state == PROCESS_STATE_RUNNING) {
            cur->state = PROCESS_STATE_TERMINATED;
            cur->signal_pending = 0;
            unblock_parent(cur->pid);
        }
    }

    if(cur_proccess_id != -1 && process_table[cur_proccess_id].state == PROCESS_STATE_RUNNING) {
        process_table[cur_proccess_id].state = PROCESS_STATE_READY;
    }
    int original_process_id = cur_proccess_id;
    do {
        cur_proccess_id = (cur_proccess_id + 1) % num_processes;
        if(process_table[cur_proccess_id].state == PROCESS_STATE_READY || process_table[cur_proccess_id].state == PROCESS_STATE_NEW) break;
    } while(cur_proccess_id != original_process_id);

    pcb_t *next = &process_table[cur_proccess_id];
    if (next->state == PROCESS_STATE_TERMINATED) {
        current_process = NULL;
        __asm__ __volatile__("sti; hlt");
    }
    
    // printf("Switching to process %d\n", cur_proccess_id);
    // printf("EIP = %08ux, ESP = %08ux\n", next->regs.eip, next->regs.esp);
    // cnt++;
    // if(cnt>=15) return;
    
    tss_set_stack(0x10, next->kernel_stack_top);
    
    switch_to_process(next, r);
       
    // Should never reach here
    for(;;);
}

void remove_child_from_parent(pcb_t *parent, uint32_t child_pid)
{
    for (uint32_t i = 0; i < parent->num_children; i++) {
        if (parent->children_id[i] == child_pid) {
            parent->children_id[i] = parent->children_id[parent->num_children - 1];
            parent->num_children--;
            return;
        }
    }
}

/* Terminate all live children of a dying parent to prevent orphans */
void kill_children_of(uint32_t parent_pid)
{
    for (uint32_t i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].parent_id == parent_pid &&
            process_table[i].state != PROCESS_STATE_TERMINATED) {
            process_table[i].state = PROCESS_STATE_TERMINATED;
        }
    }
}

/* Returns PID of a terminated child of `parent_pid`, or 0 if none */
uint32_t find_terminated_child(uint32_t parent_pid)
{
    pcb_t *parent = get_process_by_pid(parent_pid);
    if (!parent) return 0;
    for (uint32_t i = 0; i < parent->num_children; i++) {
        pcb_t *child = get_process_by_pid(parent->children_id[i]);
        if (child && child->state == PROCESS_STATE_TERMINATED)
            return child->pid;
    }
    return 0;
}

/* Returns 1 if `parent_pid` has live (non-terminated) children */
int has_live_children(uint32_t parent_pid)
{
    pcb_t *parent = get_process_by_pid(parent_pid);
    if (!parent) return 0;
    for (uint32_t i = 0; i < parent->num_children; i++) {
        pcb_t *child = get_process_by_pid(parent->children_id[i]);
        if (child && child->state != PROCESS_STATE_TERMINATED)
            return 1;
    }
    return 0;
}

/* Free all resources of a terminated child. Must be called
 * while the child is no longer referenced by any parent's
 * children list. Switches to kernel page directory for cleanup. */
void process_cleanup_child(pcb_t *child)
{
    /* Free kernel stack */
    if (child->kernel_stack_alloc) {
        free((void *)child->kernel_stack_alloc);
        child->kernel_stack_alloc = 0;
    }

    /* Free address space */
    if (child->regs.cr3) {
        uint32_t *saved_dir = vmm_get_directory();
        switch_to_kernel_page_dir();
        vmm_free_directory((uint32_t *)child->regs.cr3);
        set_page_dir(saved_dir);
        child->regs.cr3 = 0;
    }

    /* Free FILE structs — decrement refcount, close node if last reference */
    for (int i = 0; i < MAX_FILES; i++) {
        if (child->files_open[i]) {
            FILE *fp = child->files_open[i];
            fs_node_t *node = fp->file;
            if (node && node->refcount > 0)
                node->refcount--;
            if (node && node->refcount == 0) {
                close_fs(node);
                free(node);
            }
            free(fp);
            child->files_open[i] = 0;
        }
    }

    /* Free VMA regions */
    vma_t *vma = child->memory_regions;
    while (vma) {
        vma_t *next = vma->next;
        free(vma);
        vma = next;
    }
    child->memory_regions = 0;
}

/* Duplicate the calling process. Returns pointer to child PCB (parent),
 * or NULL on error. Child's saved EAX is set to 0. */
pcb_t *fork_process(pcb_t *parent, struct regs *regs)
{
    if (num_processes >= MAX_PROCESSES)
        return NULL;

    /* Find free slot */
    uint32_t slot = num_processes;
    pcb_t *child = &process_table[slot];

    /* Clone PCB fields */
    child->pid = next_pid++;
    child->state = PROCESS_STATE_BLOCKED;
    child->parent_id = parent->pid;
    child->signal_pending = 0;
    child->num_children = 0;

    /* Copy proc_name */
    for (int i = 0; i < 19; i++)
        child->proc_name[i] = parent->proc_name[i];
    child->proc_name[19] = '\0';

    /* Copy CWD */
    int ci = 0;
    while (parent->cwd[ci] && ci < 254) {
        child->cwd[ci] = parent->cwd[ci];
        ci++;
    }
    child->cwd[ci] = '\0';

    /* Allocate new kernel stack */
    uint32_t kstack_virt = (uint32_t)malloc(KERNEL_STACK_SIZE);
    child->kernel_stack_alloc = kstack_virt;
    child->kernel_stack_bottom = kstack_virt;
    uint32_t kstack_base = kstack_virt + KERNEL_STACK_SIZE;
    child->kernel_stack_top = (kstack_base - 32) & ~0xF;

    /* Register child in parent's children list */
    parent->children_id[parent->num_children] = child->pid;
    parent->num_children++;

    /* Clone page directory + deep-copy user pages.
     * Switch to parent's page dir so we can read its pages,
     * then switch to child's so we can write new mappings. */
    uint32_t *saved_dir = vmm_get_directory();
    uint32_t *parent_dir = (uint32_t *)((uint32_t)parent->regs.cr3);
    set_page_dir(parent_dir);

    uint32_t *child_dir = vmm_clone_directory();
    set_page_dir(child_dir);

    /* Copy user pages from parent into child's address space */
    vmm_copy_user_pages(parent_dir);

    set_page_dir(saved_dir);

    child->regs.cr3 = (uint32_t)child_dir;

    /* Clone register state */
    child->regs = parent->regs;
    child->regs.eax = 0; /* fork return value in child */

    /* Clone VMA list (deep copy) */
    child->memory_regions = NULL;
    vma_t *src_vma = parent->memory_regions;
    vma_t *dst_vma_last = NULL;
    while (src_vma) {
        vma_t *new_vma = malloc(sizeof(vma_t));
        new_vma->start = src_vma->start;
        new_vma->end = src_vma->end;
        new_vma->flags = src_vma->flags;
        new_vma->next = NULL;
        if (dst_vma_last) {
            dst_vma_last->next = new_vma;
        } else {
            child->memory_regions = new_vma;
        }
        dst_vma_last = new_vma;
        src_vma = src_vma->next;
    }

    /* Share open files — increment refcount on each node */
    for (int i = 0; i < MAX_FILES; i++) {
        if (parent->files_open[i]) {
            FILE *new_fp = malloc(sizeof(FILE));
            new_fp->flags = parent->files_open[i]->flags;
            new_fp->file = parent->files_open[i]->file;
            if (new_fp->file)
                new_fp->file->refcount++;
            child->files_open[i] = new_fp;
        } else {
            child->files_open[i] = NULL;
        }
    }

    child->state = PROCESS_STATE_READY;
    num_processes++;
    return child;
}

/* Unblock the parent of `child_pid` and set its return value.
 * Does NOT free child resources or remove from children list —
 * that is done by process_cleanup_child() when the parent reaps
 * the child via wait().  Keeping the child in the children list
 * as a zombie allows a subsequent wait() to find and clean it up. */
void unblock_parent(uint32_t child_pid)
{
    pcb_t *child = get_process_by_pid(child_pid);
    if (!child) return;

    pcb_t *parent = get_process_by_pid(child->parent_id);
    if (parent && parent->state == PROCESS_STATE_BLOCKED) {
        uint32_t *tf = (uint32_t *)parent->regs.esp;
        tf[11] = child_pid; /* EAX is at offset 44 / sizeof(uint32_t) = 11 */
        parent->state = PROCESS_STATE_READY;
    }
}