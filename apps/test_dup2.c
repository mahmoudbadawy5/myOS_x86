#include <unistd.h>

int main(void)
{
    print("Step 2: dup2 + kernel dup2 test\n");

    /* Test dup2 returns correct fd */
    print("  dup2(1, 20)...");
    int d = dup2(1, 20);
    if (d == 20) {
        print(" OK\n");
    } else {
        print(" FAIL\n");
        return 1;
    }
    close(20);

    /* Test dup2 replacing existing fd */
    print("  dup2(0, 20)...");
    int d1 = dup2(0, 20);
    if (d1 == 20) {
        print(" OK\n");
    } else {
        print(" FAIL\n");
    }
    close(20);

    /* Test dup2 same fd */
    print("  dup2(1, 1)...");
    int d4 = dup2(1, 1);
    if (d4 == 1) {
        print(" OK\n");
    } else {
        print(" FAIL\n");
    }

    /* Test dup2 with invalid fd */
    print("  dup2(99, 0)...");
    int d3 = dup2(99, 0);
    if (d3 == -1) {
        print(" OK (-1)\n");
    } else {
        print(" FAIL\n");
    }

    /* Test dup2 + write: dup stdout to 20, write via print (uses fd 1),
     * verify print still works */
    print("  dup2(1,20) + print...");
    dup2(1, 20);
    print("OK\n");
    close(20);

    /* Test fork + dup2 child writes to parent's stdout */
    print("  fork + dup2...");
    int pid = fork();
    if (pid == 0) {
        /* Child: dup2 stdout to something, write */
        dup2(1, 15);
        close(15);
        print(" child OK\n");
        exit(0);
    }
    if (pid > 0) {
        wait();
        print(" parent OK\n");
    }

    print("Step 2 complete\n");
    return 0;
}
