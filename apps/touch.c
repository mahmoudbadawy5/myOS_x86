#include <test.h>
#include <syscalls.h>

int main(int argc, char **argv)
{
    if (argc < 2) {
        print("touch: missing file operand\n");
        return 1;
    }

    int fd = sys_open(argv[1], "w");
    if (fd < 0) {
        print("touch: ");
        print(argv[1]);
        print(": cannot create\n");
        return 1;
    }
    /* File created/opened; no close syscall yet */
    return 0;
}
