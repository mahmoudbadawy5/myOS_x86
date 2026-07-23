#include <test.h>
#include <syscalls.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    if (argc < 3) {
        print("grep: usage: grep PATTERN FILE\n");
        return 1;
    }

    const char *pattern = argv[1];
    const char *filename = argv[2];

    FILE *fp = fopen(filename, "r");
    if (!fp) {
        print("grep: ");
        print(filename);
        print(": no such file\n");
        return 1;
    }

    fseek(fp, 0, SEEK_END);
    int fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (fsize <= 0) {
        fclose(fp);
        return 1;
    }

    char *data = malloc(fsize);
    if (!data) {
        print("grep: out of memory\n");
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

    int found = 0;
    int plen = strlen(pattern);
    int line_start = 0;

    for (int i = 0; i <= total; i++) {
        if (i == total || data[i] == '\n') {
            int line_len = i - line_start;
            if (line_len >= plen) {
                for (int j = 0; j <= line_len - plen; j++) {
                    int match = 1;
                    for (int k = 0; k < plen; k++) {
                        if (data[line_start + j + k] != pattern[k]) {
                            match = 0;
                            break;
                        }
                    }
                    if (match) {
                        sys_write(data + line_start, line_len);
                        if (line_len > 0 && data[line_start + line_len - 1] != '\n')
                            sys_write("\n", 1);
                        found = 1;
                        break;
                    }
                }
            }
            line_start = i + 1;
        }
    }

    free(data);
    return found ? 0 : 1;
}
