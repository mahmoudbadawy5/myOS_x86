#pragma once

#include <types.h>

typedef struct
{
    uint32_t esp;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
    uint32_t esi;
    uint32_t edi;
    uint32_t ebp;
    uint32_t ds;
    uint32_t es;
    uint32_t fs;
    uint32_t gs;
    uint32_t eax;
} __attribute__((packed)) syscall_regs_t;

#define MAX_SYSCALLS 2

void init_syscalls();
int32_t syscall_test0(syscall_regs_t regs);
int32_t syscall_test1(syscall_regs_t regs);

// Defined in syscalls_asm.asm

extern void handle_syscalls();