#include <test.h>
#include <syscalls.h>
#include <stdio.h>
#include <stdlib.h>

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
    if (argc < 2) {
        print("hexdump: missing file operand\n");
        return 1;
    }

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

    if (fsize <= 0) {
        fclose(fp);
        return 0;
    }

    char *data = malloc(fsize);
    if (!data) {
        print("hexdump: out of memory\n");
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

    for (int offset = 0; offset < total; offset += 16) {
        /* Offset */
        print_hex(offset, 8);
        print("  ");

        /* Hex bytes */
        for (int i = 0; i < 16; i++) {
            if (offset + i < total)
                print_hex((unsigned char)data[offset + i], 2);
            else
                print("  ");
            if (i == 7) print(" ");
            else print(" ");
        }

        /* ASCII */
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
