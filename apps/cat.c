#include <unistd.h>
#include <syscalls.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    int fd;

    if (argc < 2) {
        fd = 0;
    } else {
        fd = open(argv[1], "r");
        if (fd < 0) {
            print("cat: ");
            print(argv[1]);
            print(": no such file\n");
            return 1;
        }
    }

    char buf[512];
    int n;
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        write(1, buf, n);
    }

    if (fd != 0)
        close(fd);
    return 0;
}
