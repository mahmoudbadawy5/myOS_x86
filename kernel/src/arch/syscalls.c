#include <types.h>
#include <stdio.h>
#include <arch/syscalls.h>
#include <idt.h>

int32_t (*syscalls[MAX_SYSCALLS])(syscall_regs_t) = {
    syscall_test0,
    syscall_test1};

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