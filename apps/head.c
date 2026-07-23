#include <test.h>
#include <syscalls.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
        print("head: missing file operand\n");
        return 1;
    }

    FILE *fp = fopen(filename, "r");
    if (!fp) {
        print("head: ");
        print(filename);
        print(": no such file\n");
        return 1;
    }

    /* Read entire file into memory */
    fseek(fp, 0, SEEK_END);
    int fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (fsize <= 0) {
        fclose(fp);
        return 0;
    }

    char *data = malloc(fsize);
    if (!data) {
        print("head: out of memory\n");
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

    /* Print first max_lines lines */
    int lines = 0;
    int i = 0;
    char outbuf[256];
    int outpos = 0;
    while (i < total && lines < max_lines) {
        outbuf[outpos++] = data[i];
        if (outpos >= 256 || data[i] == '\n') {
            sys_write(outbuf, outpos);
            outpos = 0;
        }
        if (data[i] == '\n') lines++;
        i++;
    }
    if (outpos > 0)
        sys_write(outbuf, outpos);

    free(data);
    return 0;
}
