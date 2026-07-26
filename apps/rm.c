#include <unistd.h>
#include <string.h>

int main(int argc, char **argv)
{
    if (argc < 2) {
        write(1, "rm: missing operand\n", 20);
        return 1;
    }

    if (unlink(argv[1]) < 0) {
        write(1, "rm: cannot remove '", 19);
        write(1, argv[1], strlen(argv[1]));
        write(1, "'\n", 2);
        return 1;
    }
    return 0;
}
