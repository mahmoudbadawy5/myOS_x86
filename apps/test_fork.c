#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <syscalls.h>

static void test_file_io(void)
{
    const char *path = "/hello.txt";

    printf("[pid %d] Opening %s\n", getpid(), path);
    FILE *fp = fopen(path, "r");
    if (!fp) {
        printf("[pid %d] FAIL: cannot open %s\n", getpid(), path);
        return;
    }

    char buf[64];
    int n = fread(buf, 1, 63, fp);
    if (n > 0) {
        buf[n] = '\0';
        printf("[pid %d] Read %d bytes: \"%s\"\n", getpid(), n, buf);
    } else {
        printf("[pid %d] Read returned %d\n", getpid(), n);
    }

    fclose(fp);
    printf("[pid %d] File closed OK\n", getpid());
}

int main(void)
{
    printf("=== Fork + File I/O Test ===\n\n");

    /* Test file I/O before fork */
    printf("-- file I/O before fork --\n");
    test_file_io();
    printf("\n");

    /* Fork */
    printf("-- forking --\n");
    int child_pid = fork();

    if (child_pid < 0) {
        printf("FAIL: fork returned %d\n", child_pid);
        return 1;
    }

    if (child_pid == 0) {
        /* Child */
        printf("[child] I am PID %d, fork returned %d\n", getpid(), child_pid);
        test_file_io();
        printf("[child] Done\n");
        exit(0);
    }

    /* Parent */
    printf("[parent] Fork returned child PID %d, my PID is %d\n", child_pid, getpid());
    test_file_io();

    printf("[parent] Waiting for child %d...\n", child_pid);
    int waited = wait();
    printf("[parent] wait() returned %d\n", waited);

    printf("\n=== All tests passed! ===\n");
    return 0;
}
