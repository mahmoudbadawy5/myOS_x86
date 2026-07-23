#include <types.h>
#include <stdio.h>
#include <string.h>
#include <arch/syscalls.h>
#include <idt.h>
#include <proc/process.h>
#include <proc/elf.h>
#include <mem/phys_mem.h>
#include <mem/virt_mem.h>
#include <mem/vmm.h>
#include <mem/malloc.h>
#include <fs/pipe.h>

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
    syscall_exec,
    syscall_dup,
    syscall_pipe,
    syscall_kill,
    syscall_getpid,
    syscall_lseek,
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
    if (fd >= MAX_FILES)
        return -1;
    FILE *fp = current_process->files_open[fd];
    if (!fp)
        return -1;
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
    uint32_t fd = regs->esi;
    if (fd >= MAX_FILES)
        return -1;
    FILE *fp = current_process->files_open[fd];
    if (!fp)
        return -1;
    char *cur = (char *)(regs->ebx);
    return fwrite(cur, regs->ecx, regs->edx, fp);
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

    uint32_t page = ALIGN_PAGE(old_brk);
    uint32_t last_mapped = old_brk;
    while (page < new_brk) {
        void *phys = alloc_blocks(1);
        if (!phys) {
            ERROR("sbrk: out of memory\n");
            break;
        }
        map_address_user((void*) page, phys);
        last_mapped = page + BLOCK_SIZE;
        page += BLOCK_SIZE;
    }

    heap->end = last_mapped;
    if (last_mapped < new_brk)
        return -1;
    return old_brk;
}

int32_t syscall_spawn(struct regs *regs)
{
    char *cmdline_user = (char *)regs->ebx;

    /* Copy command line to kernel buffer (user memory may vanish after page switch) */
    char cmdline[256];
    int len = 0;
    while (cmdline_user[len] && len < 255) {
        cmdline[len] = cmdline_user[len];
        len++;
    }
    cmdline[len] = '\0';

    /* Skip leading spaces */
    char *p = cmdline;
    while (*p == ' ') p++;

    /* Parse into argv */
    const char *argv[16];
    int argc = 0;
    while (*p && argc < 15) {
        argv[argc++] = p;
        while (*p && *p != ' ') p++;
        if (*p) {
            *p = '\0';
            p++;
            while (*p == ' ') p++;
        }
    }

    if (argc == 0)
        return -1;

    /* Build path: "/<argv[0]>.bin" */
    char path[128];
    path[0] = '/';
    int i;
    for (i = 0; argv[0][i] && i < 122; i++)
        path[i + 1] = argv[0][i];
    path[i + 1] = '.';
    path[i + 2] = 'b';
    path[i + 3] = 'i';
    path[i + 4] = 'n';
    path[i + 5] = '\0';

    create_process(path, current_process->pid, argc, argv);
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

/*
    exec — replace current process image with a new program.
    ebx: pointer to command line string (same format as spawn)
    Returns: does not return on success (new program runs), -1 on error.
*/
int32_t syscall_exec(struct regs *regs)
{
    char *cmdline_user = (char *)regs->ebx;

    /* Copy command line to kernel buffer */
    char cmdline[256];
    int len = 0;
    while (cmdline_user[len] && len < 255) {
        cmdline[len] = cmdline_user[len];
        len++;
    }
    cmdline[len] = '\0';

    char *p = cmdline;
    while (*p == ' ') p++;

    const char *argv[16];
    int argc = 0;
    while (*p && argc < 15) {
        argv[argc++] = p;
        while (*p && *p != ' ') p++;
        if (*p) {
            *p = '\0';
            p++;
            while (*p == ' ') p++;
        }
    }

    if (argc == 0)
        return -1;

    /* Build path: "/<argv[0]>.bin" */
    char path[128];
    path[0] = '/';
    int i;
    for (i = 0; argv[0][i] && i < 122; i++)
        path[i + 1] = argv[0][i];
    path[i + 1] = '.';
    path[i + 2] = 'b';
    path[i + 3] = 'i';
    path[i + 4] = 'n';
    path[i + 5] = '\0';

    pcb_t *proc = current_process;

    /* Free old address space */
    uint32_t *saved_dir = vmm_get_directory();
    switch_to_kernel_page_dir();
    vmm_free_directory((uint32_t *)proc->regs.cr3);
    set_page_dir(saved_dir);

    /* Free old VMA regions */
    vma_t *vma = proc->memory_regions;
    while (vma) {
        vma_t *next = vma->next;
        free(vma);
        vma = next;
    }
    proc->memory_regions = 0;

    /* Free old files (keep stdin/stdout) */
    for (int i = 2; i < MAX_FILES; i++) {
        if (proc->files_open[i]) {
            free(proc->files_open[i]);
            proc->files_open[i] = 0;
        }
    }

    /* Set up new page directory + load ELF + argv — shared helper */
    if (load_program(proc, path, argc, argv) != 0) {
        /* Load failed — process has no valid address space; terminate it */
        proc->state = PROCESS_STATE_TERMINATED;
        return -1;
    }

    return 0;
}

/*
    dup — duplicate a file descriptor.
    ebx: old fd
    Returns: new fd, or -1 on error.
*/
int32_t syscall_dup(struct regs *regs)
{
    uint32_t old_fd = regs->ebx;
    if (old_fd >= MAX_FILES)
        return -1;
    FILE *fp = current_process->files_open[old_fd];
    if (!fp)
        return -1;

    /* Find first free slot */
    for (uint32_t i = 0; i < MAX_FILES; i++) {
        if (!current_process->files_open[i]) {
            current_process->files_open[i] = fp;
            return i;
        }
    }
    return -1; /* no free slots */
}

/*
    pipe — create a pipe for inter-process communication.
    ebx: pointer to 2-element int array in user space
    Array[0] = read fd, Array[1] = write fd
    Returns: 0 on success, -1 on error.
*/
int32_t syscall_pipe(struct regs *regs)
{
    int *fds = (int *)regs->ebx;

    FILE *read_fp, *write_fp;
    if (pipe_create(&read_fp, &write_fp) != 0)
        return -1;

    /* Find two free fd slots */
    int read_fd = -1, write_fd = -1;
    for (uint32_t i = 3; i < MAX_FILES; i++) {
        if (!current_process->files_open[i]) {
            if (read_fd == -1)
                read_fd = i;
            else {
                write_fd = i;
                break;
            }
        }
    }
    if (read_fd == -1 || write_fd == -1) {
        /* read_node->ptr and write_node->ptr share the same pipe_buf_t — free only once */
        free(read_fp->file->ptr);
        free(read_fp->file);
        free(write_fp->file);
        free(read_fp);
        free(write_fp);
        return -1;
    }

    current_process->files_open[read_fd] = read_fp;
    current_process->files_open[write_fd] = write_fp;

    /* Copy fds to user space */
    fds[0] = read_fd;
    fds[1] = write_fd;

    return 0;
}

/*
    kill — send a signal to a process.
    ebx: target pid
    ecx: signal number
    Returns: 0 on success, -1 on error.
*/
int32_t syscall_kill(struct regs *regs)
{
    uint32_t target_pid = regs->ebx;
    uint32_t signal = regs->ecx;

    if (target_pid == 0 || signal == 0)
        return -1;

    pcb_t *target = get_process_by_pid(target_pid);
    if (!target)
        return -1;

    if (target->state == PROCESS_STATE_TERMINATED)
        return -1;

    target->signal_pending = signal;
    return 0;
}

/*
    getpid — get current process ID.
    Returns: current process PID.
*/
int32_t syscall_getpid(struct regs *regs)
{
    (void)regs;
    if (!current_process)
        return 0;
    return current_process->pid;
}

/*
    lseek — reposition file offset.
    ebx: file descriptor
    ecx: offset
    edx: whence (0=SEEK_SET, 1=SEEK_CUR, 2=SEEK_END)
    Returns: new offset from start, or -1 on error.
*/
int32_t syscall_lseek(struct regs *regs)
{
    uint32_t fd = regs->ebx;
    int32_t offset = (int32_t)regs->ecx;
    uint32_t whence = regs->edx;

    if (fd >= MAX_FILES)
        return -1;
    FILE *fp = current_process->files_open[fd];
    if (!fp || !fp->file)
        return -1;

    if (whence == 0) { /* SEEK_SET */
        if (offset < 0)
            return -1;
    } else if (whence == 1) { /* SEEK_CUR */
        int32_t new_off = fp->file->seek_offset + offset;
        if (offset > 0 && new_off < 0)
            return -1;
        if (offset < 0 && new_off > 0)
            return -1;
        if (new_off < 0)
            return -1;
        offset = new_off;
        whence = 0;
    } else if (whence == 2) { /* SEEK_END */
        /* offset relative to end, allow negative to mean before EOF */
    } else {
        return -1;
    }

    seek_fs(fp->file, offset, whence);
    return (int32_t)fp->file->seek_offset;
}