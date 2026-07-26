#include <test.h>
#include <syscalls.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_hex(unsigned int val, int width)
{
    char buf[9];
    int i = 0;
    for (int j = width - 1; j >= 0; j--) {
        unsigned int nibble = (val >> (j * 4)) & 0xF;
        buf[i++] = nibble < 10 ? '0' + nibble : 'a' + nibble - 10;
    }
    buf[i] = '\0';
    print(buf);
}

int main(int argc, char **argv)
{
    char *data = 0;
    int total = 0;

    if (argc >= 2) {
        FILE *fp = fopen(argv[1], "r");
        if (!fp) {
            print("hexdump: ");
            print(argv[1]);
            print(": no such file\n");
            return 1;
        }
        fseek(fp, 0, SEEK_END);
        int fsize = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        if (fsize > 0) {
            data = malloc(fsize);
            if (!data) { print("hexdump: out of memory\n"); fclose(fp); return 1; }
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
        if (!data) { print("hexdump: out of memory\n"); return 1; }
        char buf[512];
        int n;
        while ((n = sys_read_fd(0, buf, sizeof(buf))) > 0) {
            while (total + n > cap) {
                cap *= 2;
                char *tmp = realloc(data, cap);
                if (!tmp) { free(data); print("hexdump: out of memory\n"); return 1; }
                data = tmp;
            }
            memcpy(data + total, buf, n);
            total += n;
        }
    }

    if (total <= 0) { free(data); return 0; }

    for (int offset = 0; offset < total; offset += 16) {
        print_hex(offset, 8);
        print("  ");
        for (int i = 0; i < 16; i++) {
            if (offset + i < total)
                print_hex((unsigned char)data[offset + i], 2);
            else
                print("  ");
            if (i == 7) print(" ");
            else print(" ");
        }
        print(" |");
        char ascbuf[17];
        int asci = 0;
        for (int i = 0; i < 16 && offset + i < total; i++) {
            char c = data[offset + i];
            ascbuf[asci++] = (c >= 32 && c < 127) ? c : '.';
        }
        ascbuf[asci] = '\0';
        print(ascbuf);
        print("|\n");
    }

    free(data);
    return 0;
}
