#include <types.h>
#include <stdio.h>
#include <arch/syscalls.h>
#include <idt.h>
#include <proc/process.h>
#include <mem/phys_mem.h>
#include <mem/virt_mem.h>
#include <mem/vmm.h>

int32_t (*syscalls[MAX_SYSCALLS])(syscall_regs_t *) = {
    syscall_test0,
    syscall_test1,
    syscall_open,
    syscall_read,
    syscall_write,
    syscall_exit,
    syscall_yield,
    syscall_sbrk,
};

void init_syscalls(void)
{
    idt_set_gate(0x80, (unsigned)handle_syscalls, 0x08, 0xEE);
}

int32_t syscall_test0(syscall_regs_t *regs)
{
    printf("System call 0 (test): %d %d\n", regs->eax, regs->ebx);
    return 0;
}

int32_t syscall_test1(syscall_regs_t *regs)
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

int32_t syscall_open(syscall_regs_t *regs)
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

int32_t syscall_read(syscall_regs_t *regs)
{
    return fread((char *)regs->ebx, regs->ecx, regs->edx, current_process->files_open[regs->esi]);
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

int32_t syscall_write(syscall_regs_t *regs)
{
    char *cur = (char *)(regs->ebx);
    return fwrite(cur, regs->ecx, regs->edx, current_process->files_open[regs->esi]);
}

int32_t syscall_exit(syscall_regs_t *regs)
{
    (void)regs;
    if (current_process) {
        current_process->state = PROCESS_STATE_TERMINATED;
        current_process = NULL;
        schedule(regs);
    }
    return 0;
}

int32_t syscall_yield(syscall_regs_t *regs)
{
    (void)regs;
    schedule(regs);
    return 0;
}


int32_t syscall_sbrk(syscall_regs_t *regs)
{
    int32_t increment = (int32_t)regs->ebx;

    vma_t *heap = vmm_find_area_by_flag(current_process->memory_regions, VMA_HEAP);
    if(!heap) {
        ERROR("Process has no heap");
        return -1;
    }
    uint32_t old_brk = heap->end;
    uint32_t new_brk = old_brk + increment;

    printf("old_brk: %08ux\n",old_brk);
    // map new pages
    uint32_t *old_dir = vmm_get_directory();
    set_page_dir((uint32_t*) current_process->regs.cr3);

    uint32_t page = ALIGN_PAGE(old_brk);
    while (page < new_brk) {
        map_address_user(page, alloc_blocks(1));
        page += BLOCK_SIZE;
    }

    heap->end = new_brk;
    // set_page_dir(old_dir);
    return old_brk;
}