#include <test.h>
#include <syscalls.h>

int main(int argc, char **argv)
{
    if (argc < 2) {
        print("rm: missing operand\n");
        return 1;
    }

    if (sys_unlink(argv[1]) < 0) {
        print("rm: cannot remove '");
        print(argv[1]);
        print("'\n");
        return 1;
    }
    return 0;
}
