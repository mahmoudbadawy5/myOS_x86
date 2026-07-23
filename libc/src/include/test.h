#pragma once

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <syscalls.h>

/* Legacy names for existing apps */
unsigned int print(const char *msg);
unsigned int read(char *buf, int len);
void exit(int status);
unsigned int test_syscall0(unsigned int val);
unsigned int test_syscall1(unsigned int val);
unsigned int sbrk(unsigned int increament);
unsigned int spawn(const char *path);
unsigned int wait(void);
unsigned int exec(const char *cmdline);
int dup(int fd);
int pipe(int fds[2]);
int kill(int pid, int signal);
int getpid(void);
