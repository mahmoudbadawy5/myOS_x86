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