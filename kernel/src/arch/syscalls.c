#include <types.h>
#include <stdio.h>
#include <arch/syscalls.h>
#include <idt.h>
#include <proc/process.h>
#include <mem/phys_mem.h>
#include <mem/virt_mem.h>
#include <mem/vmm.h>

int32_t (*syscalls[MAX_SYSCALLS])(struct regs *) = {
    syscall_test0,
    syscall_test1,
    syscall_open,
    syscall_read,
    syscall_write,
    syscall_exit,
    syscall_yield,
    syscall_sbrk,
    syscall_spawn,
    syscall_wait,
};

void init_syscalls(void)
{
    idt_set_gate(0x80, (unsigned)handle_syscalls, 0x08, 0xEE);
}

int32_t syscall_test0(struct regs *regs)
{
    printf("System call 0 (test): %d %d\n", regs->eax, regs->ebx);
    return 0;
}

int32_t syscall_test1(struct regs *regs)
{
    printf("System call 1 (test): %d %d\n", regs->eax, regs->ebx);
    return 15;
}

/*
    Inputs:
        ebx: Pointer to char* file name
        ecx: Pointer to char* modes
    Output:
        File id or -2 if file is not found
*/

int32_t syscall_open(struct regs *regs)
{
    return fopen((char *)regs->ebx, (char *)regs->ecx);
}

/*
    Inputs:
        ebx: buf
        ecx: size
        edx count
        esi: file no
    Output:
        count
*/

int32_t syscall_read(struct regs *regs)
{
    uint32_t fd = regs->esi;
    FILE *fp = current_process->files_open[fd];
    int ret = fread((char *)regs->ebx, regs->ecx, regs->edx, fp);
    return ret;
}

/*
    Inputs:
        ebx: buf
        ecx: size
        edx count
        esi: file no
    Output:
        count
*/

int32_t syscall_write(struct regs *regs)
{
    char *cur = (char *)(regs->ebx);
    return fwrite(cur, regs->ecx, regs->edx, current_process->files_open[regs->esi]);
}

int32_t syscall_exit(struct regs *regs)
{
    (void)regs;
    if (current_process) {
        /* Kill all live children so they don't become orphans */
        kill_children_of(current_process->pid);

        current_process->state = PROCESS_STATE_TERMINATED;
        unblock_parent(current_process->pid);
        current_process = NULL;
        schedule(regs);
    }
    return 0;
}

int32_t syscall_yield(struct regs *regs)
{
    (void)regs;
    schedule(regs);
    return 0;
}


int32_t syscall_sbrk(struct regs *regs)
{
    int32_t increment = (int32_t)regs->ebx;

    vma_t *heap = vmm_find_area_by_flag(current_process->memory_regions, VMA_HEAP);
    if(!heap) {
        ERROR("Process has no heap");
        return -1;
    }
    uint32_t old_brk = heap->end;
    uint32_t new_brk = old_brk + increment;

    // map new pages
    uint32_t *old_dir = vmm_get_directory();
    set_page_dir((uint32_t*) current_process->regs.cr3);

    uint32_t page = ALIGN_PAGE(old_brk);
    while (page < new_brk) {
        void *phys = alloc_blocks(1);
        if (!phys) {
            ERROR("sbrk: out of memory\n");
            break;
        }
        map_address_user((void*) page, phys);
        page += BLOCK_SIZE;
    }

    heap->end = new_brk;
    set_page_dir(old_dir);
    return old_brk;
}

int32_t syscall_spawn(struct regs *regs)
{
    char *path = (char *)regs->ebx;
    create_process(path, current_process->pid);
    return 0;
}

int32_t syscall_wait(struct regs *regs)
{
    (void)regs;
    if (!current_process)
        return -1;

    uint32_t my_pid = current_process->pid;

    /* Reap any already-terminated child immediately */
    uint32_t dead = find_terminated_child(my_pid);
    if (dead != 0) {
        remove_child_from_parent(current_process, dead);
        pcb_t *child = get_process_by_pid(dead);
        if (child) {
            process_cleanup_child(child);
            child->state = PROCESS_STATE_TERMINATED; /* mark slot reusable */
        }
        regs->eax = dead;
        return dead;
    }

    /* No terminated child — if we have live children, block */
    if (has_live_children(my_pid)) {
        current_process->state = PROCESS_STATE_BLOCKED;
        schedule(regs);
        /* schedule() never returns — unblock_parent() writes the
         * child PID directly into our saved trap frame EAX slot */
    }

    /* No children at all */
    return -1;
}