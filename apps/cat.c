#include <test.h>
#include <syscalls.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    if (argc < 2) {
        print("cat: missing file operand\n");
        return 1;
    }

    int fd = sys_open(argv[1], "r");
    if (fd < 0) {
        print("cat: ");
        print(argv[1]);
        print(": no such file\n");
        return 1;
    }

    char buf[512];
    int n;
    while ((n = sys_read_fd(fd, buf, sizeof(buf))) > 0) {
        sys_write(buf, n);
    }

    /* No close syscall yet; fd cleaned up on process exit */
    return 0;
}
