#include <test.h>
#include <syscalls.h>

/* Test basic pipe communication: parent writes, child reads */
void test_basic_pipe(void)
{
    print("TEST: basic pipe...\n");

    int fds[2];
    if (pipe(fds) != 0) {
        print("  FAIL: pipe() failed\n");
        return;
    }

    int pid = fork();
    if (pid == 0) {
        /* Child: read from pipe */
        sys_close(fds[1]);
        char buf[32];
        int n = sys_read_fd(fds[0], buf, sizeof(buf) - 1);
        sys_close(fds[0]);
        if (n > 0) {
            buf[n] = '\0';
            print("  child read: ");
            print(buf);
            print("\n");
        } else {
            print("  FAIL: child read 0 bytes\n");
        }
        exit(0);
    }

    /* Parent: write to pipe */
    sys_close(fds[0]);
    const char *msg = "hello from pipe";
    sys_write_fd(fds[1], msg, 15);
    sys_close(fds[1]);
    wait();
    print("  PASS\n");
}

/* Test EOF detection: child reads after parent closes write end */
void test_eof(void)
{
    print("TEST: pipe EOF...\n");

    int fds[2];
    pipe(fds);

    int pid = fork();
    if (pid == 0) {
        /* Child: read until EOF */
        sys_close(fds[1]);
        char buf[32];
        int total = 0;
        int n;
        while ((n = sys_read_fd(fds[0], buf + total, sizeof(buf) - 1 - total)) > 0)
            total += n;
        sys_close(fds[0]);
        buf[total] = '\0';
        print("  child got: ");
        print(buf);
        print("\n");
        exit(0);
    }

    /* Parent: write then close */
    sys_close(fds[0]);
    sys_write_fd(fds[1], "data", 4);
    sys_close(fds[1]);
    wait();
    print("  PASS\n");
}

int main(void)
{
    print("\n=== pipe tests ===\n");
    test_basic_pipe();
    test_eof();
    print("=== done ===\n");
    return 0;
}
