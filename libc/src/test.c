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