#include <unistd.h>
#include <string.h>

int main(int argc, char **argv)
{
    if (argc < 2) {
        write(1, "touch: missing file operand\n", 28);
        return 1;
    }

    int fd = open(argv[1], "w");
    if (fd < 0) {
        write(1, "touch: ", 7);
        write(1, argv[1], strlen(argv[1]));
        write(1, ": cannot create\n", 16);
        return 1;
    }
    close(fd);
    return 0;
}
