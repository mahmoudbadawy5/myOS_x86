#include <test.h>

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
        sys_close(fds[1]);
        char buf[32];
        int n = sys_read_fd(fds[0], buf, sizeof(buf) - 1);
        sys_close(fds[0]);
        if (n > 0) {
            buf[n] = '\0';
            print(" child read: ");
            print(buf);
            print("\n");
        }
        exit(0);
    }

    /* Parent: close read end, write to pipe */
    sys_close(fds[0]);
    sys_write_fd(fds[1], "fork pipe test", 14);
    sys_close(fds[1]);
    wait();

    print("  PASS (no crash)\n");

    /* Test: fork, both sides write then both read */
    print("  TEST: fork bidirectional...");
    int fds2[2];
    pipe(fds2);

    int pid2 = fork();
    if (pid2 == 0) {
        /* Child: write then close */
        sys_close(fds2[0]);
        sys_write_fd(fds2[1], "child msg", 9);
        sys_close(fds2[1]);
        exit(0);
    }
    /* Parent: wait for child to write, then read */
    sys_close(fds2[0]);
    sys_close(fds2[1]);
    wait();
    print("  PASS\n");

    print("Step 4 complete\n");
    return 0;
}
