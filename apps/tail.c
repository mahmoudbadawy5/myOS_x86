#include <test.h>
#include <syscalls.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    int max_lines = 10;
    const char *filename = 0;

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1] == 'n' && argv[i][2] == '\0' && i + 1 < argc) {
            int n = 0;
            char *s = argv[i + 1];
            while (*s >= '0' && *s <= '9') { n = n * 10 + (*s - '0'); s++; }
            if (n > 0) max_lines = n;
            i++;
        } else {
            filename = argv[i];
        }
    }

    if (!filename) {
        print("tail: missing file operand\n");
        return 1;
    }

    FILE *fp = fopen(filename, "r");
    if (!fp) {
        print("tail: ");
        print(filename);
        print(": no such file\n");
        return 1;
    }

    fseek(fp, 0, SEEK_END);
    int fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (fsize <= 0) {
        fclose(fp);
        return 0;
    }

    char *data = malloc(fsize);
    if (!data) {
        print("tail: out of memory\n");
        fclose(fp);
        return 1;
    }

    int total = 0;
    while (total < fsize) {
        int n = fread(data + total, 1, fsize - total, fp);
        if (n <= 0) break;
        total += n;
    }
    fclose(fp);

    /* Count newlines from end to find start position */
    int nl_count = 0;
    int pos = total - 1;
    while (pos >= 0 && nl_count < max_lines) {
        if (data[pos] == '\n') nl_count++;
        pos--;
    }
    pos++; /* move past the last newline we didn't count */

    /* If we didn't find enough newlines, start from beginning */
    if (nl_count < max_lines)
        pos = 0;
    else
        pos++; /* skip the newline that terminated the count */

    sys_write(data + pos, total - pos);
    free(data);
    return 0;
}
