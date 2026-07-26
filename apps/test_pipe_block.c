#include <unistd.h>
#include <string.h>

int main(void)
{
    print("Step 3: pipe data flow test\n");

    /* Test 1: basic write + read through pipe */
    print("  TEST: write then read...");
    int fds[2];
    pipe(fds);

    int pid = fork();
    if (pid == 0) {
        close(fds[1]);
        char buf[32];
        int n = read(fds[0], buf, sizeof(buf) - 1);
        close(fds[0]);
        if (n > 0) {
            buf[n] = '\0';
            if (strcmp(buf, "hello pipe") == 0) {
                print(" PASS\n");
            } else {
                print(" wrong data FAIL\n");
            }
        } else {
            print(" 0 bytes FAIL\n");
        }
        exit(0);
    }
    close(fds[0]);
    write(fds[1], "hello pipe", 10);
    close(fds[1]);
    wait();

    /* Test 2: EOF when writer closes */
    print("  TEST: EOF detection...");
    int fds2[2];
    pipe(fds2);

    int pid2 = fork();
    if (pid2 == 0) {
        close(fds2[1]);
        char buf[32];
        int n = read(fds2[0], buf, sizeof(buf) - 1);
        close(fds2[0]);
        if (n == 0) {
            print(" PASS\n");
        } else {
            print(" got data instead of EOF FAIL\n");
        }
        exit(0);
    }
    close(fds2[0]);
    close(fds2[1]);
    wait();

    print("Step 3 complete\n");
    return 0;
}
