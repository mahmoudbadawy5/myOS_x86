#include <test.h>

unsigned int test_syscall0(unsigned int val)
{
    unsigned int ret;
    __asm__ __volatile__("int $0x80;"
                         : "=a"(ret)
                         : "a"(0), "b"(val));
    return ret;
}

unsigned int test_syscall1(unsigned int val)
{
    unsigned int ret;
    __asm__ __volatile__("int $0x80;"
                         : "=a"(ret)
                         : "a"(1), "b"(val));
    return ret;
}

int strlen(const char *c)
{
    int i = 0;
    while (c[i])
        i++;
    return i;
}

unsigned int read(char* buf, int len)
{
    unsigned int ret;
    __asm__ __volatile__("int $0x80;"
                         : "=a"(ret)
                         : "a"(3), "b"(buf), "c"(1), "d"(len), "S"(0));
    return ret;
}

unsigned int print(const char *msg)
{
    unsigned int ret;
    __asm__ __volatile__("int $0x80;"
                         : "=a"(ret)
                         : "a"(4), "b"(msg), "c"(1), "d"(strlen(msg)), "S"(1));
    return ret;
}

unsigned int exit() {
    unsigned int ret;
    __asm__ __volatile__("int $0x80;"
                         : "=a"(ret)
                         : "a"(5));
    return ret;
}

unsigned int sbrk(unsigned int increament) {
    unsigned int ret;
    __asm__ __volatile__("int $0x80;"
                         : "=a"(ret)
                         : "a"(7), "b"(increament));
    return ret;
}

unsigned int spawn(const char *path) {
    unsigned int ret;
    __asm__ __volatile__("int $0x80;"
                         : "=a"(ret)
                         : "a"(8), "b"(path));
    return ret;
}

unsigned int wait() {
    unsigned int ret;
    __asm__ __volatile__("int $0x80;"
                         : "=a"(ret)
                         : "a"(9));
    return ret;
}