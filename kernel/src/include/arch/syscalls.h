#pragma once

#include <types.h>
#include <isr.h>

#define MAX_SYSCALLS 20

void init_syscalls(void);
int32_t syscall_test0(struct regs *regs);
int32_t syscall_test1(struct regs *regs);
int32_t syscall_open(struct regs *regs);
int32_t syscall_read(struct regs *regs);
int32_t syscall_write(struct regs *regs);
int32_t syscall_exit(struct regs *regs);
int32_t syscall_yield(struct regs *regs);
int32_t syscall_sbrk(struct regs *regs);
int32_t syscall_spawn(struct regs *regs);
int32_t syscall_wait(struct regs *regs);
int32_t syscall_exec(struct regs *regs);
int32_t syscall_dup(struct regs *regs);
int32_t syscall_pipe(struct regs *regs);
int32_t syscall_kill(struct regs *regs);
int32_t syscall_getpid(struct regs *regs);
int32_t syscall_lseek(struct regs *regs);
int32_t syscall_readdir(struct regs *regs);
int32_t syscall_stat(struct regs *regs);
int32_t syscall_getcwd(struct regs *regs);
int32_t syscall_chdir(struct regs *regs);

// Defined in syscalls_asm.asm

extern void handle_syscalls();
