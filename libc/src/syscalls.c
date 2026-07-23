#include <syscalls.h>

int sys_test0(int val)
{
    int ret;
    __asm__ __volatile__("int $0x80" : "=a"(ret) : "a"(0), "b"(val) : "memory", "cc");
    return ret;
}

int sys_test1(int val)
{
    int ret;
    __asm__ __volatile__("int $0x80" : "=a"(ret) : "a"(1), "b"(val) : "memory", "cc");
    return ret;
}

int sys_open(const char *path, const char *mode)
{
    int ret;
    __asm__ __volatile__("int $0x80" : "=a"(ret) : "a"(2), "b"(path), "c"(mode) : "memory", "cc");
    return ret;
}

int sys_read(void *buf, int len)
{
    int ret;
    __asm__ __volatile__("int $0x80" : "=a"(ret) : "a"(3), "b"(buf), "c"(1), "d"(len), "S"(0) : "memory", "cc");
    return ret;
}

int sys_read_fd(int fd, void *buf, int len)
{
    int ret;
    __asm__ __volatile__("int $0x80" : "=a"(ret) : "a"(3), "b"(buf), "c"(fd), "d"(len), "S"(0) : "memory", "cc");
    return ret;
}

int sys_write(const char *buf, int len)
{
    int ret;
    __asm__ __volatile__("int $0x80" : "=a"(ret) : "a"(4), "b"(buf), "c"(1), "d"(len), "S"(1) : "memory", "cc");
    return ret;
}

int sys_write_fd(int fd, const char *buf, int len)
{
    int ret;
    __asm__ __volatile__("int $0x80" : "=a"(ret) : "a"(4), "b"(buf), "c"(fd), "d"(len), "S"(1) : "memory", "cc");
    return ret;
}

void sys_exit(void)
{
    __asm__ __volatile__("int $0x80" : : "a"(5) : "memory", "cc");
    for(;;);
}

void *sys_sbrk(unsigned int increment)
{
    void *ret;
    __asm__ __volatile__("int $0x80" : "=a"(ret) : "a"(7), "b"(increment) : "memory", "cc");
    return ret;
}

int sys_spawn(const char *path)
{
    int ret;
    __asm__ __volatile__("int $0x80" : "=a"(ret) : "a"(8), "b"(path) : "memory", "cc");
    return ret;
}

int sys_wait(void)
{
    int ret;
    __asm__ __volatile__("int $0x80" : "=a"(ret) : "a"(9) : "memory", "cc");
    return ret;
}

int sys_exec(const char *cmdline)
{
    int ret;
    __asm__ __volatile__("int $0x80" : "=a"(ret) : "a"(10), "b"(cmdline) : "memory", "cc");
    return ret;
}

int sys_dup(int fd)
{
    int ret;
    __asm__ __volatile__("int $0x80" : "=a"(ret) : "a"(11), "b"(fd) : "memory", "cc");
    return ret;
}

int sys_pipe(int fds[2])
{
    int ret;
    __asm__ __volatile__("int $0x80" : "=a"(ret) : "a"(12), "b"(fds) : "memory", "cc");
    return ret;
}

int sys_kill(int pid, int signal)
{
    int ret;
    __asm__ __volatile__("int $0x80" : "=a"(ret) : "a"(13), "b"(pid), "c"(signal) : "memory", "cc");
    return ret;
}

int sys_getpid(void)
{
    int ret;
    __asm__ __volatile__("int $0x80" : "=a"(ret) : "a"(14) : "memory", "cc");
    return ret;
}

int sys_lseek(int fd, int offset, int whence)
{
    int ret;
    __asm__ __volatile__("int $0x80" : "=a"(ret) : "a"(15), "b"(fd), "c"(offset), "d"(whence) : "memory", "cc");
    return ret;
}
