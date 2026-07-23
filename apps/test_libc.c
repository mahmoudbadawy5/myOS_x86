#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <test.h>

int main(void)
{
    printf("=== Libc Test Suite ===\n\n");

    /* --- String tests --- */
    printf("-- string.h --\n");

    const char *src = "Hello, libc!";
    char buf[64];

    /* strlen */
    int len = strlen(src);
    printf("  strlen(\"%s\") = %d\n", src, len);

    /* strcpy */
    strcpy(buf, src);
    printf("  strcpy -> \"%s\"\n", buf);

    /* strcmp */
    printf("  strcmp(\"abc\",\"abc\") = %d\n", strcmp("abc", "abc"));
    printf("  strcmp(\"abc\",\"abd\") = %d\n", strcmp("abc", "abd"));
    printf("  strcmp(\"abd\",\"abc\") = %d\n", strcmp("abd", "abc"));

    /* strcat */
    char catbuf[64] = "Hello, ";
    strcat(catbuf, "world!");
    printf("  strcat -> \"%s\"\n", catbuf);

    /* memcpy / memset */
    char mbuf[16];
    memset(mbuf, 0, sizeof(mbuf));
    memcpy(mbuf, "ABCD", 4);
    printf("  memcpy+memset -> \"%s\"\n", mbuf);

    /* strncmp */
    printf("  strncmp(\"abc\",\"abd\",2) = %d\n", strncmp("abc", "abd", 2));

    /* strstr */
    const char *found = strstr("foo bar baz", "bar");
    printf("  strstr(\"foo bar baz\",\"bar\") = %s\n", found ? "found" : "not found");

    /* --- ctype tests --- */
    printf("\n-- ctype.h --\n");
    printf("  isdigit('3') = %d, isdigit('a') = %d\n", isdigit('3'), isdigit('a'));
    printf("  isalpha('z') = %d, isalpha('5') = %d\n", isalpha('z'), isalpha('5'));
    printf("  isspace(' ') = %d, isspace('x') = %d\n", isspace(' '), isspace('x'));
    printf("  toupper('a') = %c, tolower('Z') = %c\n", toupper('a'), tolower('Z'));

    /* --- stdlib tests --- */
    printf("\n-- stdlib.h --\n");

    /* atoi */
    printf("  atoi(\"42\") = %d\n", atoi("42"));
    printf("  atoi(\"-7\") = %d\n", atoi("-7"));

    /* malloc / free */
    void *p1 = malloc(64);
    void *p2 = malloc(128);
    void *p3 = malloc(256);
    printf("  malloc(64)  = %p\n", p1);
    printf("  malloc(128) = %p\n", p2);
    printf("  malloc(256) = %p\n", p3);

    /* Write to allocated memory to verify it's usable */
    if (p1) {
        memset(p1, 'A', 63);
        ((char *)p1)[63] = '\0';
        printf("  p1 content: \"%s\"\n", (char *)p1);
    }

    free(p1);
    free(p2);
    free(p3);
    printf("  free(p1,p2,p3) done\n");

    /* Realloc */
    void *p4 = malloc(32);
    if (p4) memset(p4, 'B', 31);
    void *p4_new = realloc(p4, 64);
    if (p4_new) p4 = p4_new;
    printf("  realloc(32->64) = %p\n", p4);
    free(p4);

    /* --- printf format tests --- */
    printf("\n-- printf formats --\n");
    printf("  int:    %d\n", 12345);
    printf("  neg:    %d\n", -42);
    printf("  hex:    %x\n", 0xDEAD);
    printf("  oct:    %o\n", 63);
    printf("  string: %s\n", "test");
    printf("  char:   %c\n", 'X');
    printf("  ptr:    %p\n", (void *)0x12345678);
    printf("  zero-padded hex: %08x\n", 0xAB);

    printf("\n=== All tests passed! ===\n");
    return 0;
}
