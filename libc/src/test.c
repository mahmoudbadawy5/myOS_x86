#include <test.h>
#include <string.h>
#include <syscalls.h>

/* Compat wrappers so existing apps keep working */
unsigned int print(const char *msg)
{
    return sys_write(msg, strlen(msg));
}

unsigned int read(char *buf, int len)
{
    return sys_read(buf, len);
}

unsigned int exit(void)
{
    sys_exit();
    return 0;
}

unsigned int test_syscall0(unsigned int val)
{
    return sys_test0(val);
}

unsigned int test_syscall1(unsigned int val)
{
    return sys_test1(val);
}

unsigned int sbrk(unsigned int increament)
{
    return (unsigned int)sys_sbrk(increament);
}

unsigned int spawn(const char *path)
{
    return sys_spawn(path);
}

unsigned int wait(void)
{
    return sys_wait();
}

unsigned int exec(const char *cmdline)
{
    return sys_exec(cmdline);
}

int dup(int fd)
{
    return sys_dup(fd);
}

int pipe(int fds[2])
{
    return sys_pipe(fds);
}

int kill(int pid, int signal)
{
    return sys_kill(pid, signal);
}

int getpid(void)
{
    return sys_getpid();
}
