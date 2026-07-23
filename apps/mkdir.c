#include <test.h>
#include <syscalls.h>

int main(int argc, char **argv)
{
    if (argc < 2) {
        print("mkdir: missing operand\n");
        return 1;
    }

    if (sys_mkdir(argv[1]) < 0) {
        print("mkdir: cannot create directory '");
        print(argv[1]);
        print("'\n");
        return 1;
    }
    return 0;
}
