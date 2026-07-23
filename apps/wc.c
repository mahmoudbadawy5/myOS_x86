#include <test.h>
#include <syscalls.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    if (argc < 2) {
        print("wc: missing file operand\n");
        return 1;
    }

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

    if (fsize <= 0) {
        print("0 0 0 ");
        print(argv[1]);
        print("\n");
        fclose(fp);
        return 0;
    }

    char *data = malloc(fsize);
    if (!data) {
        print("wc: out of memory\n");
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

    /* Print numbers */
    char buf[64];
    int pos = 0;
    /* lines */
    int v = lines;
    char tmp[12]; int ti = 0;
    if (v == 0) { tmp[ti++] = '0'; }
    else { while (v) { tmp[ti++] = '0' + v % 10; v /= 10; } }
    while (ti) buf[pos++] = tmp[--ti];
    buf[pos++] = ' ';
    /* words */
    v = words; ti = 0;
    if (v == 0) { tmp[ti++] = '0'; }
    else { while (v) { tmp[ti++] = '0' + v % 10; v /= 10; } }
    while (ti) buf[pos++] = tmp[--ti];
    buf[pos++] = ' ';
    /* bytes */
    v = bytes; ti = 0;
    if (v == 0) { tmp[ti++] = '0'; }
    else { while (v) { tmp[ti++] = '0' + v % 10; v /= 10; } }
    while (ti) buf[pos++] = tmp[--ti];
    buf[pos++] = ' ';
    /* filename */
    int fnlen = 0;
    while (argv[1][fnlen]) buf[pos++] = argv[1][fnlen++];
    buf[pos++] = '\n';
    buf[pos] = '\0';

    sys_write(buf, pos);
    return 0;
}
