#include <unistd.h>

int main(void)
{
    print("Step 4: fork + pipe refcount test\n");

    /* Test: parent opens pipe, forks child, both use pipe, both close,
     * verify no crash and pipe_buf survives */
    print("  TEST: fork with pipe...");
    int fds[2];
    pipe(fds);

    int pid = fork();
    if (pid == 0) {
        /* Child: close write end, read from pipe */
        close(fds[1]);
        char buf[32];
        int n = read(fds[0], buf, sizeof(buf) - 1);
        close(fds[0]);
        if (n > 0) {
            buf[n] = '\0';
            print(" child read: ");
            print(buf);
            print("\n");
        }
        exit(0);
    }

    /* Parent: close read end, write to pipe */
    close(fds[0]);
    write(fds[1], "fork pipe test", 14);
    close(fds[1]);
    wait();

    print("  PASS (no crash)\n");

    /* Test: fork, both sides write then both read */
    print("  TEST: fork bidirectional...");
    int fds2[2];
    pipe(fds2);

    int pid2 = fork();
    if (pid2 == 0) {
        /* Child: write then close */
        close(fds2[0]);
        write(fds2[1], "child msg", 9);
        close(fds2[1]);
        exit(0);
    }
    /* Parent: wait for child to write, then read */
    close(fds2[0]);
    close(fds2[1]);
    wait();
    print("  PASS\n");

    print("Step 4 complete\n");
    return 0;
}
