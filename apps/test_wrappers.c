#include <unistd.h>

int main(void)
{
    print("Step 1: libc wrappers test\n");

    /* Test fork */
    print("  fork()...");
    int pid = fork();
    if (pid == 0) {
        print(" child OK\n");
        exit(0);
    }
    if (pid > 0) {
        print(" parent OK (child=");
        /* print pid as decimal */
        char buf[12];
        int n = 0;
        int tmp = pid;
        if (tmp == 0) { buf[n++] = '0'; }
        else {
            char rev[12];
            int rn = 0;
            while (tmp > 0) { rev[rn++] = '0' + (tmp % 10); tmp /= 10; }
            while (rn > 0) { buf[n++] = rev[--rn]; }
        }
        buf[n] = '\0';
        print(buf);
        print(")\n");
        wait();
    }
    if (pid < 0) {
        print(" FAIL\n");
    }

    /* Test dup */
    print("  dup(0)...");
    int d = dup(0);
    if (d >= 0) {
        print(" OK (fd=");
        char buf[12];
        int n = 0;
        int tmp = d;
        if (tmp == 0) { buf[n++] = '0'; }
        else {
            char rev[12];
            int rn = 0;
            while (tmp > 0) { rev[rn++] = '0' + (tmp % 10); tmp /= 10; }
            while (rn > 0) { buf[n++] = rev[--rn]; }
        }
        buf[n] = '\0';
        print(buf);
        print(")\n");
        close(d);
    } else {
        print(" FAIL\n");
    }

    /* Test dup2 */
    print("  dup2(0, 20)...");
    int d2 = dup2(0, 20);
    if (d2 == 20) {
        print(" OK\n");
        close(20);
    } else {
        print(" FAIL\n");
    }

    /* Test pipe */
    print("  pipe()...");
    int fds[2];
    int pr = pipe(fds);
    if (pr == 0 && fds[0] >= 0 && fds[1] >= 0) {
        print(" OK (read=");
        char buf[12];
        int n = 0;
        int tmp = fds[0];
        if (tmp == 0) { buf[n++] = '0'; }
        else {
            char rev[12];
            int rn = 0;
            while (tmp > 0) { rev[rn++] = '0' + (tmp % 10); tmp /= 10; }
            while (rn > 0) { buf[n++] = rev[--rn]; }
        }
        buf[n] = '\0';
        print(buf);
        print(", write=");
        n = 0;
        tmp = fds[1];
        if (tmp == 0) { buf[n++] = '0'; }
        else {
            char rev[12];
            int rn = 0;
            while (tmp > 0) { rev[rn++] = '0' + (tmp % 10); tmp /= 10; }
            while (rn > 0) { buf[n++] = rev[--rn]; }
        }
        buf[n] = '\0';
        print(buf);
        print(")\n");
        close(fds[0]);
        close(fds[1]);
    } else {
        print(" FAIL\n");
    }

    print("Step 1 complete\n");
    return 0;
}
