#include <unistd.h>
#include <string.h>

int main(int argc, char **argv)
{
    if (argc < 2) {
        write(1, "mkdir: missing operand\n", 23);
        return 1;
    }

    if (mkdir(argv[1]) < 0) {
        write(1, "mkdir: cannot create directory '", 31);
        write(1, argv[1], strlen(argv[1]));
        write(1, "'\n", 2);
        return 1;
    }
    return 0;
}
