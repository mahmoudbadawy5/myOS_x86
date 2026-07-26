#include <test.h>
#include <syscalls.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    int fd;

    if (argc < 2) {
        fd = 0;
    } else {
        fd = sys_open(argv[1], "r");
        if (fd < 0) {
            print("cat: ");
            print(argv[1]);
            print(": no such file\n");
            return 1;
        }
    }

    char buf[512];
    int n;
    while ((n = sys_read_fd(fd, buf, sizeof(buf))) > 0) {
        sys_write(buf, n);
    }

    if (fd != 0)
        sys_close(fd);
    return 0;
}
