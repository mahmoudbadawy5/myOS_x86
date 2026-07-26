#include <unistd.h>
#include <string.h>
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
    else if (getcwd(cwd_buf, sizeof(cwd_buf)) != 0)
        path = "/";

    char *entries = malloc(MAX_ENTRIES * NAME_LEN);
    if (!entries) {
        write(1, "ls: out of memory\n", 18);
        return 1;
    }

    int count = readdir(path, entries, MAX_ENTRIES);

    if (count < 0) {
        write(1, "ls: cannot access '", 19);
        write(1, path, strlen(path));
        write(1, "'\n", 2);
        free(entries);
        return 1;
    }

    for (int i = 0; i < count; i++) {
        write(1, entries + i * NAME_LEN, strlen(entries + i * NAME_LEN));
        write(1, "\n", 1);
    }

    free(entries);
    return 0;
}
