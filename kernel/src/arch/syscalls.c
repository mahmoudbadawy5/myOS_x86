#include <types.h>
#include <stdio.h>
#include <string.h>
#include <arch.h>
#include <arch/syscalls.h>
#include <idt.h>
#include <proc/process.h>
#include <proc/elf.h>
#include <mem/phys_mem.h>
#include <mem/virt_mem.h>
#include <mem/vmm.h>
#include <mem/malloc.h>
#include <fs/pipe.h>

/* Validate that a pointer lies in userspace (below kernel base) */
static inline int is_user_ptr(const void *p)
{
    return (uint32_t)p < KERNEL_VIRTUAL_BASE && p != 0;
}

void resolve_path(const char *user_path, char *buf, int buf_size)
{
    if (!user_path || !buf || buf_size <= 0) {
        if (buf && buf_size > 0) buf[0] = '\0';
        return;
    }

    /* Copy user path to kernel with safe copy */
    char path[256];
    int len = 0;
    while (len < 255) {
        char c;
        if (copy_from_user(&c, user_path + len, 1) != 0) {
            buf[0] = '\0';
            return;
        }
        if (c == '\0') break;
        path[len++] = c;
    }
    path[len] = '\0';

    /* Build absolute path: start with cwd if relative */
    char abs[256];
    int ai = 0;
    if (path[0] != '/') {
        char *cwd = current_process->cwd;
        while (cwd[ai] && ai < 254)
            abs[ai] = cwd[ai], ai++;
        if (ai > 0 && abs[ai - 1] != '/')
            abs[ai++] = '/';
    }
    int pi = 0;
    while (path[pi] && ai < 254)
        abs[ai++] = path[pi++];
    abs[ai] = '\0';

    /* Normalize: resolve . and .. */
    char norm[256];
    int ni = 0;
    int i = 0;
    while (abs[i]) {
        /* Skip slashes */
        if (abs[i] == '/') {
            i++;
            continue;
        }
        /* Find end of component */
        int start = i;
        while (abs[i] && abs[i] != '/')
            i++;
        int clen = i - start;

        if (clen == 1 && abs[start] == '.') {
            /* Skip . */
        } else if (clen == 2 && abs[start] == '.' && abs[start + 1] == '.') {
            /* Go up: remove last component */
            if (ni > 0) {
                ni--; /* remove trailing slash */
                while (ni > 0 && norm[ni - 1] != '/')
                    ni--;
            }
        } else {
            if (ni < 255)
                norm[ni++] = '/';
            for (int j = start; j < i && ni < 255; j++)
                norm[ni++] = abs[j];
        }
    }
    if (ni == 0) {
        norm[ni++] = '/';
    }
    norm[ni] = '\0';

    /* Copy to output */
    int ci = 0;
    while (norm[ci] && ci < buf_size - 1) {
        buf[ci] = norm[ci];
        ci++;
    }
    buf[ci] = '\0';
}

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
    syscall_readdir,
    syscall_stat,
    syscall_getcwd,
    syscall_chdir,
    syscall_close,
    syscall_mkdir,
    syscall_unlink,
    syscall_ps,
    syscall_fork,
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
    char path[256];
    resolve_path((char *)regs->ebx, path, sizeof(path));

    char mode[8];
    if (copy_from_user(mode, (void *)regs->ecx, sizeof(mode)) != 0) {
        mode[0] = 'r';
        mode[1] = '\0';
    } else {
        mode[sizeof(mode) - 1] = '\0';
    }

    return fopen(path, mode);
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
    char *buf = (char *)regs->ebx;
    uint32_t size = regs->ecx;
    uint32_t count = regs->edx;

    if (fd >= MAX_FILES)
        return -1;
    FILE *fp = current_process->files_open[fd];
    if (!fp)
        return -1;

    /* Validate user buffer is mapped */
    uint32_t total = size * count;
    if (count != 0 && total / count != size)
        return -1; /* overflow */
    for (uint32_t a = (uint32_t)buf; a < (uint32_t)buf + total; a += 4096) {
        if (!is_page_mapped(a))
            return -1;
    }

    int ret = fread(buf, size, count, fp);
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
    char *buf = (char *)(regs->ebx);
    uint32_t size = regs->ecx;
    uint32_t count = regs->edx;

    if (fd >= MAX_FILES)
        return -1;
    FILE *fp = current_process->files_open[fd];
    if (!fp)
        return -1;

    /* Validate user buffer is mapped */
    uint32_t total = size * count;
    if (count != 0 && total / count != size)
        return -1; /* overflow */
    for (uint32_t a = (uint32_t)buf; a < (uint32_t)buf + total; a += 4096) {
        if (!is_page_mapped(a))
            return -1;
    }

    return fwrite(buf, size, count, fp);
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

    if (!is_user_ptr(fds))
        return -1;

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
        free(read_fp->file->ptr);
        free(read_fp->file);
        free(write_fp->file);
        free(read_fp);
        free(write_fp);
        return -1;
    }

    current_process->files_open[read_fd] = read_fp;
    current_process->files_open[write_fd] = write_fp;

    /* Copy fds to user space safely */
    int kfds[2] = { read_fd, write_fd };
    if (copy_to_user(fds, kfds, sizeof(kfds)) != 0)
        return -1;

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

/*
    readdir — list directory entries.
    ebx: path string (user space)
    ecx: pointer to user buffer (char names[][256])
    edx: max entries
    Returns: number of entries, or -1 on error.
*/
int32_t syscall_readdir(struct regs *regs)
{
    char *path_user = (char *)regs->ebx;
    char *buf_user = (char *)regs->ecx;
    uint32_t max_entries = regs->edx;

    if (!is_user_ptr(path_user) || !is_user_ptr(buf_user) || max_entries == 0)
        return -1;

    char path[256];
    resolve_path(path_user, path, sizeof(path));

    fs_node_t *node = get_node(path, root_dir);
    if (!node)
        return -1;

    dirent_t *dire = readdir_fs(node);
    if (!dire)
        return -1;

    uint32_t count = dire->file_count;
    if (count > max_entries)
        count = max_entries;

    /* Build kernel buffer, then validate and copy to user */
    uint32_t total = count * 256;
    char *kbuf = malloc(total);
    if (!kbuf) {
        free(dire->files);
        free(dire);
        return -1;
    }

    for (uint32_t i = 0; i < count; i++) {
        int nlen = 0;
        while (dire->files[i][nlen] && nlen < 254)
            nlen++;
        for (int j = 0; j <= nlen; j++)
            kbuf[i * 256 + j] = dire->files[i][j];
    }
    free(dire->files);
    free(dire);

    if (copy_to_user(buf_user, kbuf, total) != 0) {
        free(kbuf);
        return -1;
    }
    free(kbuf);

    return (int32_t)count;
}

/*
    stat — get file info.
    ebx: path string (user space)
    ecx: pointer to stat_result in user space: { uint32_t size, uint32_t type }
    Returns: 0 on success, -1 on error.
*/
int32_t syscall_stat(struct regs *regs)
{
    char *path_user = (char *)regs->ebx;
    uint32_t *stat_buf = (uint32_t *)regs->ecx;

    if (!is_user_ptr(path_user) || !is_user_ptr(stat_buf))
        return -1;

    char path[256];
    resolve_path(path_user, path, sizeof(path));

    fs_node_t *node = get_node(path, root_dir);
    if (!node)
        return -1;

    uint32_t kbuf[2];
    kbuf[0] = node->size;
    kbuf[1] = (node->flags & FS_DIRECTORY) ? 1 : 0;

    if (copy_to_user(stat_buf, kbuf, sizeof(kbuf)) != 0)
        return -1;

    return 0;
}

/*
    getcwd — get current working directory.
    ebx: buffer pointer (user space)
    ecx: buffer size
    Returns: 0 on success, -1 on error.
*/
int32_t syscall_getcwd(struct regs *regs)
{
    char *buf_user = (char *)regs->ebx;
    uint32_t buf_size = regs->ecx;

    if (!is_user_ptr(buf_user) || buf_size == 0)
        return -1;

    char *cwd = current_process->cwd;
    int len = 0;
    while (cwd[len] && len < (int)buf_size - 1)
        len++;

    if (copy_to_user(buf_user, cwd, len + 1) != 0)
        return -1;

    return 0;
}

/*
    chdir — change current working directory.
    ebx: path string (user space)
    Returns: 0 on success, -1 on error.
*/
int32_t syscall_chdir(struct regs *regs)
{
    char *path_user = (char *)regs->ebx;
    if (!is_user_ptr(path_user))
        return -1;

    char path[256];
    resolve_path(path_user, path, sizeof(path));

    fs_node_t *node = get_node(path, root_dir);
    if (!node)
        return -1;
    if (!(node->flags & FS_DIRECTORY))
        return -1;

    /* Store resolved absolute path as cwd */
    int i = 0;
    while (path[i] && i < 254) {
        current_process->cwd[i] = path[i];
        i++;
    }
    current_process->cwd[i] = '\0';

    return 0;
}

/*
    close — close a file descriptor.
    ebx: file descriptor
    Returns: 0 on success, -1 on error.
*/
int32_t syscall_close(struct regs *regs)
{
    uint32_t fd = regs->ebx;
    if (fd < 3 || fd >= MAX_FILES)
        return -1;
    if (!current_process->files_open[fd])
        return -1;

    FILE *fp = current_process->files_open[fd];
    fs_node_t *node = fp->file;
    if (node && node->refcount > 0)
        node->refcount--;
    if (node && node->refcount == 0) {
        close_fs(node);
        free(node);
    }
    free(fp);
    current_process->files_open[fd] = 0;
    return 0;
}

/*
    mkdir — create a directory.
    ebx: path string (user space)
    Returns: 0 on success, -1 on error.
*/
int32_t syscall_mkdir(struct regs *regs)
{
    char *path_user = (char *)regs->ebx;
    if (!is_user_ptr(path_user))
        return -1;

    char path[256];
    resolve_path(path_user, path, sizeof(path));

    /* Find parent path and new dir name */
    int last_slash = -1;
    int len = 0;
    while (path[len]) {
        if (path[len] == '/')
            last_slash = len;
        len++;
    }

    char *dir_name = path + last_slash + 1;
    char *parent_path = "/";
    fs_node_t *parent = get_node(parent_path, root_dir);
    if (last_slash > 0) {
        /* Has a parent */
        char tmp[256];
        int pi = 0;
        while (pi < last_slash && pi < 255) {
            tmp[pi] = path[pi];
            pi++;
        }
        tmp[pi] = '\0';
        parent = get_node(tmp, root_dir);
    }
    if (!parent)
        return -1;
    if ((parent->flags & FS_MOUNTPOINT) == FS_MOUNTPOINT)
        parent = parent->ptr;

    if (parent->mkdir)
        return parent->mkdir(parent, dir_name);
    return -1;
}

/*
    unlink — remove a file.
    ebx: path string (user space)
    Returns: 0 on success, -1 on error.
*/
int32_t syscall_unlink(struct regs *regs)
{
    char *path_user = (char *)regs->ebx;
    if (!is_user_ptr(path_user))
        return -1;

    char path[256];
    resolve_path(path_user, path, sizeof(path));

    int last_slash = -1;
    int len = 0;
    while (path[len]) {
        if (path[len] == '/')
            last_slash = len;
        len++;
    }

    if (last_slash < 0)
        return -1;

    char *file_name = path + last_slash + 1;
    char *parent_path = "/";
    fs_node_t *parent = get_node(parent_path, root_dir);
    if (last_slash > 0) {
        char tmp[256];
        int pi = 0;
        while (pi < last_slash && pi < 255) {
            tmp[pi] = path[pi];
            pi++;
        }
        tmp[pi] = '\0';
        parent = get_node(tmp, root_dir);
    }
    if (!parent)
        return -1;
    if ((parent->flags & FS_MOUNTPOINT) == FS_MOUNTPOINT)
        parent = parent->ptr;

    if (parent->unlink)
        return parent->unlink(parent, file_name);
    return -1;
}

/*
    ps — list running processes.
    ebx: pointer to user buffer (ps_entry_t array)
    ecx: max entries
    Returns: number of processes, or -1 on error.
*/
int32_t syscall_ps(struct regs *regs)
{
    uint8_t *buf_user = (uint8_t *)regs->ebx;
    uint32_t max_entries = regs->ecx;

    if (!is_user_ptr(buf_user) || max_entries == 0)
        return -1;

    /* ps_entry_t: { uint32_t pid, uint32_t state, char name[20], uint32_t parent_id } = 32 bytes */
    uint8_t kbuf[32 * 10]; /* max 10 processes */
    uint32_t count = 0;
    for (uint32_t i = 0; i < MAX_PROCESSES && count < max_entries; i++) {
        pcb_t *p = &process_table[i];
        if (p->state == PROCESS_STATE_NEW)
            continue;

        uint32_t offset = count * 32;
        uint32_t pid = p->pid;
        uint32_t state = p->state;
        uint32_t parent = p->parent_id;

        for (int j = 0; j < 4; j++)
            kbuf[offset + j] = (pid >> (j * 8)) & 0xFF;
        for (int j = 0; j < 4; j++)
            kbuf[offset + 4 + j] = (state >> (j * 8)) & 0xFF;
        for (int j = 0; j < 20; j++)
            kbuf[offset + 8 + j] = p->proc_name[j];
        for (int j = 0; j < 4; j++)
            kbuf[offset + 28 + j] = (parent >> (j * 8)) & 0xFF;

        count++;
    }

    /* Validate entire user buffer range is mapped before writing */
    if (copy_to_user(buf_user, kbuf, count * 32) != 0)
        return -1;

    return (int32_t)count;
}

/*
    fork — duplicate the current process.
    Returns: 0 in child, child PID in parent, -1 on error.
*/
int32_t syscall_fork(struct regs *regs)
{
    (void)regs;
    if (!current_process)
        return -1;

    pcb_t *child = fork_process(current_process, regs);
    if (!child)
        return -1;

    return (int32_t)child->pid;
}