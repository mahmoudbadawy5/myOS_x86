#include <unistd.h>

/* Test userland signal handler delivery.
 *
 * Flow:
 *   1. Child registers a handler for SIGINT via signal()
 *   2. Child loops, yielding CPU
 *   3. Parent sends SIGINT to child
 *   4. Kernel delivers signal: saves context, redirects to handler
 *   5. Handler writes "PASS" to /hand.txt, then calls sigreturn()
 *   6. sigreturn() restores original context, child continues
 *   7. Parent reads /hand.txt to verify
 */

static void print_num(unsigned int val)
{
    char buf[11];
    int i = 0;
    if (val == 0) { buf[i++] = '0'; }
    else {
        char tmp[10]; int ti = 0;
        while (val) { tmp[ti++] = '0' + val % 10; val /= 10; }
        while (ti) { buf[i++] = tmp[--ti]; }
    }
    buf[i] = '\0';
    print(buf);
}

static void yield_now(void)
{
    yield();
    yield();
}

/* The signal handler — will be called when SIGINT is delivered.
 * Writes proof to a file, then returns. sigreturn restores context. */
void sigint_handler(int signum)
{
    (void)signum;
    int fd = open("/mnt/hand.txt", "w");
    if (fd >= 0) {
        write(fd, "SIGNAL_PASS", 11);
        close(fd);
    }
    /* Return normally — the trampoline calls sigreturn() for us */
}

int main(void)
{
    print("=== Signal Handler Test ===\n");

    /* Step 1: Register handler for SIGINT (signal 2) */
    print("Registering SIGINT handler...\n");
    signal(2, sigint_handler);

    int child_pid = fork();
    if (child_pid == 0) {
        /* Child: loop forever, yielding.  When SIGINT arrives,
         * the kernel redirects us to sigint_handler, which writes
         * the file, then sigreturn restores us here. */
        for (;;) {
            yield();
        }
        return 0;
    }

    print("Child PID: ");
    print_num(child_pid);
    print("\n");

    /* Give child a chance to start and register */
    yield_now();
    yield_now();

    /* Step 2: Send SIGINT (signal 2) to the child */
    print("Sending SIGINT to child...\n");
    kill(child_pid, 2);

    /* Let the scheduler deliver the signal */
    yield_now();
    yield_now();
    yield_now();
    yield_now();

    /* Step 3: Parent checks if the handler wrote the file */
    char buf[32] = {0};
    int fd = open("/mnt/hand.txt", "r");
    if (fd >= 0) {
        int n = read(fd, buf, 31);
        close(fd);
        if (n >= 0)
            buf[n] = '\0';
    }

    if (fd >= 0 && buf[0] == 'S' && buf[1] == 'I') {
        print("Handler wrote file: ");
        print(buf);
        print("  PASS\n");
    } else {
        print("Handler did NOT write file  FAIL\n");
    }

    /* Cleanup: kill child and remove file */
    kill(child_pid, 9);
    yield_now();
    yield_now();
    wait();
    unlink("/mnt/hand.txt");

    print("=== Signal Handler Test Done ===\n");
    return 0;
}
