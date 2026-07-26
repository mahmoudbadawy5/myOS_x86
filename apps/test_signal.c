#include <unistd.h>
#include <syscalls.h>

/* Self-contained signal test — runs from a single shell prompt.
 * Forks a child, sends signals via kill(), checks results via ps(). */

typedef struct {
    unsigned int pid;
    unsigned int state;
    char name[20];
    unsigned int parent_id;
} ps_entry_t;

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

static const char *state_name(unsigned int state)
{
    switch (state) {
        case 0: return "new";
        case 1: return "ready";
        case 2: return "running";
        case 3: return "blocked";
        case 4: return "terminated";
        case 5: return "STOPPED";
        default: return "?";
    }
}

/* Yield the CPU to let the scheduler run other processes */
static void yield_now(void)
{
    yield();
}

int main(void)
{
    print("=== Signal Test (self-contained) ===\n");

    int child_pid = fork();
    if (child_pid == 0) {
        /* Child: loop forever */
        for (;;);
        return 0;
    }

    print("Child PID: ");
    print_num(child_pid);
    print("\n");

    /* --- Test 1: SIGSTOP --- */
    print("\n[Test 1] Sending SIGSTOP (17)...\n");
    kill(child_pid, 17);  /* SIGSTOP */
    yield_now();
    yield_now();

    ps_entry_t entries[10];
    int count = ps(entries, 10);
    for (int i = 0; i < count; i++) {
        if (entries[i].pid == (unsigned int)child_pid) {
            print("  Child state: ");
            print(state_name(entries[i].state));
            if (entries[i].state == 5)
                print("  PASS\n");
            else
                print("  FAIL (expected STOPPED)\n");
            break;
        }
    }

    /* --- Test 2: SIGCONT --- */
    print("\n[Test 2] Sending SIGCONT (18)...\n");
    kill(child_pid, 18);  /* SIGCONT */
    yield_now();
    yield_now();

    count = ps(entries, 10);
    for (int i = 0; i < count; i++) {
        if (entries[i].pid == (unsigned int)child_pid) {
            print("  Child state: ");
            print(state_name(entries[i].state));
            if (entries[i].state != 5)
                print("  PASS\n");
            else
                print("  FAIL (expected not STOPPED)\n");
            break;
        }
    }

    /* --- Test 3: SIGKILL --- */
    print("\n[Test 3] Sending SIGKILL (9)...\n");
    kill(child_pid, 9);  /* SIGKILL */
    yield_now();
    yield_now();

    count = ps(entries, 10);
    int found = 0;
    for (int i = 0; i < count; i++) {
        if (entries[i].pid == (unsigned int)child_pid) {
            print("  Child state: ");
            print(state_name(entries[i].state));
            if (entries[i].state == 4)
                print("  PASS\n");
            else
                print("  FAIL (expected terminated)\n");
            found = 1;
            break;
        }
    }
    if (!found)
        print("  Child not in ps (reaped)  PASS\n");

    /* Reap the child */
    wait();

    print("\n=== All tests done ===\n");
    return 0;
}
