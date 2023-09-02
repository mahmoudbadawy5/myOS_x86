#include <types.h>
#include <stdio.h>
#include <arch/syscalls.h>
#include <idt.h>

int32_t (*syscalls[MAX_SYSCALLS])(syscall_regs_t) = {
    syscall_test0,
    syscall_test1,
    syscall_open,
    syscall_read,
    syscall_write};

void init_syscalls()
{
    // for (int i = 0; i < MAX_SYSCALLS; i++)
    idt_set_gate(0x80, (unsigned)handle_syscalls, 0x08, 0x8E);
}

int32_t syscall_test0(syscall_regs_t regs)
{
    printf("System call 0 (test): %d %d\n", regs.eax, regs.ebx);
    return 0;
}

int32_t syscall_test1(syscall_regs_t regs)
{
    printf("System call 1 (test): %d %d\n", regs.eax, regs.ebx);
    return 0;
}

/*
    Inputs:
        ebx: Pointer to char* file name
        ecx: Pointer to char* modes
    Output:
        File id or -2 if file is not found
*/

int32_t syscall_open(syscall_regs_t regs)
{
    return fopen((char *)regs.ebx, (char *)regs.ecx);
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

int32_t syscall_read(syscall_regs_t regs)
{
    return fread((char *)regs.ebx, regs.ecx, regs.edx, files_open[regs.esi]);
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

int32_t syscall_write(syscall_regs_t regs)
{
    // Dirty hack until I can load programs properly
    char *cur = (char *)(regs.ebx);
    return fwrite(cur, regs.ecx, regs.edx, files_open[regs.esi]);
}
