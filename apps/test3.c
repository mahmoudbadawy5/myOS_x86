#include <test.h>


unsigned int test_invalid_syscall(unsigned int val)
{
    // print("Hello World");
    // exit(0);
    unsigned int ret;
    // __asm__ __volatile__("mov $0,%eax; 0: ; test %eax,%eax; jz 0b");
    __asm__ __volatile__("int $0x80;"
                         : "=a"(ret)
                         : "a"(15), "b"(val));
    return ret;
}


void main()
{
    print("Hello World");
    test_invalid_syscall(15);
    print("Ay 7aga");
    int x = test_syscall1(12);
    test_syscall0(x);
    // print("7aga tanya");
    // //char buf[2] = {};
    while(1)
    {
        //print("7aga tanya");
        //if(read(buf, 1)) print(buf);
    }
}