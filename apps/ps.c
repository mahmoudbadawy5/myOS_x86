#include <test.h>
#include <syscalls.h>

typedef struct {
    unsigned int pid;
    unsigned int state;
    char name[20];
    unsigned int parent_id;
} ps_entry_t;

int main(int argc, char **argv)
{
    (void)argc; (void)argv;
    ps_entry_t entries[10];
    int count = sys_ps(entries, 10);

    if (count < 0) {
        print("ps: failed\n");
        return 1;
    }

    print("  PID NAME             STATE  PPID\n");
    for (int i = 0; i < count; i++) {
        const char *state_str;
        switch (entries[i].state) {
            case 0: state_str = "new"; break;
            case 1: state_str = "ready"; break;
            case 2: state_str = "running"; break;
            case 3: state_str = "blocked"; break;
            case 4: state_str = "terminated"; break;
            default: state_str = "?"; break;
        }

        /* Manually build the line */
        /* PID */
        unsigned int pid = entries[i].pid;
        char pid_buf[11];
        int pi = 0;
        if (pid == 0) { pid_buf[pi++] = '0'; }
        else {
            char tmp[10]; int ti = 0;
            while (pid) { tmp[ti++] = '0' + pid % 10; pid /= 10; }
            while (ti) { pid_buf[pi++] = tmp[--ti]; }
        }
        pid_buf[pi] = '\0';

        /* Pad PID to 4 chars */
        for (int j = pi; j < 4; j++) print(" ");
        print(pid_buf);
        print(" ");

        /* Name */
        print(entries[i].name);
        /* Pad name to 18 chars */
        int nlen = 0;
        while (entries[i].name[nlen] && nlen < 18) nlen++;
        for (int j = nlen; j < 18; j++) print(" ");
        print(" ");

        /* State */
        print(state_str);
        int slen = 0;
        while (state_str[slen]) slen++;
        for (int j = slen; j < 10; j++) print(" ");
        print(" ");

        /* Parent PID */
        unsigned int ppid = entries[i].parent_id;
        char ppid_buf[11];
        int ppi = 0;
        if (ppid == 0) { ppid_buf[ppi++] = '0'; }
        else {
            char tmp[10]; int ti = 0;
            while (ppid) { tmp[ti++] = '0' + ppid % 10; ppid /= 10; }
            while (ti) { ppid_buf[ppi++] = tmp[--ti]; }
        }
        ppid_buf[ppi] = '\0';
        print(ppid_buf);
        print("\n");
    }

    return 0;
}
