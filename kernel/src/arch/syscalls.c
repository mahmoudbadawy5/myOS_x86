#include <types.h>
#include <stdio.h>
#include <arch/syscalls.h>
#include <idt.h>
#include <process.h>

int32_t (*syscalls[MAX_SYSCALLS])(syscall_regs_t *) = {
    syscall_test0,
    syscall_test1,
    syscall_open,
    syscall_read,
    syscall_write,
    syscall_exit,
    syscall_yield};

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
    return fread((char *)regs->ebx, regs->ecx, regs->edx, files_open[regs->esi]);
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
    return fwrite(cur, regs->ecx, regs->edx, files_open[regs->esi]);
}

int32_t syscall_exit(syscall_regs_t *regs)
{
    (void)regs;
    if (current_process) {
        current_process->state = PROCESS_STATE_TERMINATED;
        current_process = NULL;
        schedule();
    }
    return 0;
}

int32_t syscall_yield(syscall_regs_t *regs)
{
    (void)regs;
    schedule();
    return 0;
}
