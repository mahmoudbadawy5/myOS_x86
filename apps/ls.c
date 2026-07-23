#include <test.h>
#include <syscalls.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    const char *path = "/";
    if (argc > 1)
        path = argv[1];

    char entries[128][256];
    int count = sys_readdir(path, (char *)entries, 128);

    if (count < 0) {
        print("ls: cannot access '");
        print(path);
        print("'\n");
        return 1;
    }

    for (int i = 0; i < count; i++) {
        print(entries[i]);
        print("\n");
    }

    return 0;
}
