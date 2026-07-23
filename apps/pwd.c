#include <test.h>
#include <syscalls.h>

int main(void)
{
    char buf[256];
    if (sys_getcwd(buf, sizeof(buf)) == 0)
        print(buf);
    else
        print("/");
    print("\n");
    return 0;
}
