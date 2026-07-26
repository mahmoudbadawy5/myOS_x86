#include <test.h>
#include <syscalls.h>
#include <stdio.h>
#include <stdlib.h>

static int read_all_stdin(char **out)
{
    int cap = 512;
    char *buf = malloc(cap);
    if (!buf) return -1;
    int total = 0;
    char tmp[512];
    int n;
    while ((n = sys_read_fd(0, tmp, sizeof(tmp))) > 0) {
        while (total + n > cap) {
            cap *= 2;
            char *new_buf = realloc(buf, cap);
            if (!new_buf) { free(buf); return -1; }
            buf = new_buf;
        }
        for (int i = 0; i < n; i++) buf[total + i] = tmp[i];
        total += n;
    }
    *out = buf;
    return total;
}

int main(int argc, char **argv)
{
    char *data = 0;
    int total = 0;
    const char *label = "stdin";

    if (argc >= 2) {
        FILE *fp = fopen(argv[1], "r");
        if (!fp) {
            print("wc: ");
            print(argv[1]);
            print(": no such file\n");
            return 1;
        }
        fseek(fp, 0, SEEK_END);
        int fsize = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        label = argv[1];
        if (fsize > 0) {
            data = malloc(fsize);
            if (!data) { print("wc: out of memory\n"); fclose(fp); return 1; }
            while (total < fsize) {
                int n = fread(data + total, 1, fsize - total, fp);
                if (n <= 0) break;
                total += n;
            }
        }
        fclose(fp);
    } else {
        total = read_all_stdin(&data);
        if (total < 0) { print("wc: out of memory\n"); return 1; }
    }

    int lines = 0, words = 0, bytes = total;
    int in_word = 0;
    for (int i = 0; i < total; i++) {
        if (data[i] == '\n') lines++;
        if (data[i] == ' ' || data[i] == '\n' || data[i] == '\t') {
            in_word = 0;
        } else if (!in_word) {
            words++;
            in_word = 1;
        }
    }

    free(data);

    char buf[128];
    int pos = 0;
    int v;
    char tmp[12]; int ti;

    v = lines; ti = 0;
    if (v == 0) { tmp[ti++] = '0'; }
    else { while (v) { tmp[ti++] = '0' + v % 10; v /= 10; } }
    while (ti) buf[pos++] = tmp[--ti];
    buf[pos++] = ' ';

    v = words; ti = 0;
    if (v == 0) { tmp[ti++] = '0'; }
    else { while (v) { tmp[ti++] = '0' + v % 10; v /= 10; } }
    while (ti) buf[pos++] = tmp[--ti];
    buf[pos++] = ' ';

    v = bytes; ti = 0;
    if (v == 0) { tmp[ti++] = '0'; }
    else { while (v) { tmp[ti++] = '0' + v % 10; v /= 10; } }
    while (ti) buf[pos++] = tmp[--ti];
    buf[pos++] = ' ';

    int fnlen = 0;
    while (label[fnlen] && pos < 126) buf[pos++] = label[fnlen++];
    buf[pos++] = '\n';
    buf[pos] = '\0';

    sys_write(buf, pos);
    return 0;
}
