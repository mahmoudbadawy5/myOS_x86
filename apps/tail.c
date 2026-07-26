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

    char *data = 0;
    int total = 0;

    if (filename) {
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
        if (fsize > 0) {
            data = malloc(fsize);
            if (!data) { print("tail: out of memory\n"); fclose(fp); return 1; }
            while (total < fsize) {
                int n = fread(data + total, 1, fsize - total, fp);
                if (n <= 0) break;
                total += n;
            }
        }
        fclose(fp);
    } else {
        int cap = 512;
        data = malloc(cap);
        if (!data) { print("tail: out of memory\n"); return 1; }
        char buf[512];
        int n;
        while ((n = sys_read_fd(0, buf, sizeof(buf))) > 0) {
            while (total + n > cap) {
                cap *= 2;
                char *tmp = realloc(data, cap);
                if (!tmp) { free(data); print("tail: out of memory\n"); return 1; }
                data = tmp;
            }
            memcpy(data + total, buf, n);
            total += n;
        }
    }

    if (total <= 0) { free(data); return 0; }

    /* Skip trailing newline before counting lines */
    int end = total - 1;
    if (end >= 0 && data[end] == '\n') end--;

    int nl_count = 0;
    int pos = end;
    while (pos >= 0 && nl_count < max_lines) {
        if (data[pos] == '\n') nl_count++;
        if (nl_count < max_lines) pos--;
    }

    if (nl_count < max_lines)
        pos = 0;
    else
        pos++;

    sys_write(data + pos, total - pos);
    free(data);
    return 0;
}
