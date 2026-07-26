#include <test.h>
#include <syscalls.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    if (argc < 2) {
        print("grep: usage: grep PATTERN [FILE]\n");
        return 1;
    }

    const char *pattern = argv[1];
    char *data = 0;
    int total = 0;
    int capacity = 0;

    if (argc >= 3) {
        FILE *fp = fopen(argv[2], "r");
        if (!fp) {
            print("grep: ");
            print(argv[2]);
            print(": no such file\n");
            return 1;
        }
        fseek(fp, 0, SEEK_END);
        int fsize = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        if (fsize <= 0) { fclose(fp); return 1; }
        data = malloc(fsize);
        if (!data) { print("grep: out of memory\n"); fclose(fp); return 1; }
        while (total < fsize) {
            int n = fread(data + total, 1, fsize - total, fp);
            if (n <= 0) break;
            total += n;
        }
        fclose(fp);
    } else {
        capacity = 512;
        data = malloc(capacity);
        if (!data) { print("grep: out of memory\n"); return 1; }
        char buf[512];
        int n;
        while ((n = sys_read_fd(0, buf, sizeof(buf))) > 0) {
            while (total + n > capacity) {
                capacity *= 2;
                data = realloc(data, capacity);
                if (!data) { print("grep: out of memory\n"); return 1; }
            }
            memcpy(data + total, buf, n);
            total += n;
        }
    }

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
