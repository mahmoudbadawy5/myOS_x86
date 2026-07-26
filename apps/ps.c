#include <unistd.h>
#include <string.h>

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
    int count = ps(entries, 10);

    if (count < 0) {
        write(1, "ps: failed\n", 11);
        return 1;
    }

    write(1, "  PID NAME             STATE  PPID\n", 34);
    for (int i = 0; i < count; i++) {
        const char *state_str;
        switch (entries[i].state) {
            case 0: state_str = "new"; break;
            case 1: state_str = "ready"; break;
            case 2: state_str = "running"; break;
            case 3: state_str = "blocked"; break;
            case 4: state_str = "terminated"; break;
            case 5: state_str = "stopped"; break;
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
        for (int j = pi; j < 4; j++) write(1, " ", 1);
        write(1, pid_buf, strlen(pid_buf));
        write(1, " ", 1);

        /* Name */
        write(1, entries[i].name, strlen(entries[i].name));
        /* Pad name to 18 chars */
        int nlen = 0;
        while (entries[i].name[nlen] && nlen < 18) nlen++;
        for (int j = nlen; j < 18; j++) write(1, " ", 1);
        write(1, " ", 1);

        /* State */
        write(1, state_str, strlen(state_str));
        int slen = 0;
        while (state_str[slen]) slen++;
        for (int j = slen; j < 10; j++) write(1, " ", 1);
        write(1, " ", 1);

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
        write(1, ppid_buf, strlen(ppid_buf));
        write(1, "\n", 1);
    }

    return 0;
}
