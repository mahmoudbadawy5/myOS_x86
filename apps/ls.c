#include <test.h>
#include <syscalls.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_ENTRIES 64
#define NAME_LEN 256

int main(int argc, char **argv)
{
    static char cwd_buf[256];
    const char *path = cwd_buf;
    if (argc > 1)
        path = argv[1];
    else if (sys_getcwd(cwd_buf, sizeof(cwd_buf)) != 0)
        path = "/";

    char *entries = malloc(MAX_ENTRIES * NAME_LEN);
    if (!entries) {
        print("ls: out of memory\n");
        return 1;
    }

    int count = sys_readdir(path, entries, MAX_ENTRIES);

    if (count < 0) {
        print("ls: cannot access '");
        print(path);
        print("'\n");
        free(entries);
        return 1;
    }

    for (int i = 0; i < count; i++) {
        print(entries + i * NAME_LEN);
        print("\n");
    }

    free(entries);
    return 0;
}
