#include <unistd.h>
#include <string.h>

#define LINE_MAX 256
#define MAX_ARGS 16
#define MAX_STAGES 4

/* A single command stage: args + optional redirections */
typedef struct {
    char *args[MAX_ARGS];
    int argc;
    char *in_file;
    char *out_file;
    int append; /* 1 for >> */
} stage_t;

void print_prompt(void)
{
    print("\x1b\x0FmyOS> ");
}

int read_line(char *buf, int max)
{
    int i = 0;
    char c;
    while (i < max - 1)
    {
        if (read(0, &c, 1))
        {
            if (c == '\n' || c == '\r')
            {
                buf[i] = '\0';
                print("\n");
                return i;
            }
            else if (c == '\b' || c == 0x7F)
            {
                if (i > 0)
                {
                    i--;
                    print("\b \b");
                }
            }
            else
            {
                char out[2] = {c, 0};
                print(out);
                buf[i++] = c;
            }
        }
    }
    buf[i] = '\0';
    return i;
}

/* Parse a line into pipe stages.
 * Returns number of stages (1+), or 0 on empty.
 * Each stage has its args, in_file, out_file, append set. */
int parse_line(char *line, stage_t *stages)
{
    int num_stages = 0;
    char *p = line;

    /* Skip leading spaces */
    while (*p == ' ') p++;
    if (*p == '\0') return 0;

    while (*p && num_stages < MAX_STAGES) {
        stage_t *st = &stages[num_stages];
        st->argc = 0;
        st->in_file = 0;
        st->out_file = 0;
        st->append = 0;

        /* Parse one stage: collect args, detect < > >> */
        while (*p && *p != '|') {
            /* Skip spaces */
            while (*p == ' ') p++;
            if (*p == '\0' || *p == '|') break;

            if (*p == '<') {
                /* Input redirect */
                p++;
                while (*p == ' ') p++;
                st->in_file = p;
                while (*p && *p != ' ' && *p != '|' && *p != '>' && *p != '<') p++;
                if (*p) { *p = '\0'; p++; }
            } else if (*p == '>') {
                /* Output redirect: > or >> */
                p++;
                if (*p == '>') { st->append = 1; p++; }
                while (*p == ' ') p++;
                st->out_file = p;
                while (*p && *p != ' ' && *p != '|' && *p != '>' && *p != '<') p++;
                if (*p) { *p = '\0'; p++; }
            } else {
                /* Regular argument */
                if (st->argc < MAX_ARGS - 1)
                    st->args[st->argc++] = p;
                while (*p && *p != ' ' && *p != '|' && *p != '>' && *p != '<') p++;
                if (*p) { *p = '\0'; p++; }
            }
        }
        st->args[st->argc] = 0;

        if (st->argc > 0)
            num_stages++;

        /* Skip past pipe operator */
        if (*p == '|') {
            p++;
            while (*p == ' ') p++;
        }
    }

    return num_stages;
}

void run_command(int argc, char **args);

static int is_builtin(stage_t *st)
{
    if (st->argc == 0) return 0;
    return (strcmp(st->args[0], "help") == 0 ||
            strcmp(st->args[0], "clear") == 0 ||
            strcmp(st->args[0], "cd") == 0 ||
            strcmp(st->args[0], "pwd") == 0);
}

/* Run a single stage (command with redirections) in a child process.
 * If pipe_in/pipe_out are set, they override stdin/stdout.
 * If extra_fd >= 0, close it in the child (non-adjacent pipe end).
 * If builtin, run in child after redirections instead of exec.
 * Returns child PID, or -1 on fork failure. */
int run_stage(stage_t *st, int pipe_in, int pipe_out, int extra_fd)
{
    int pid = fork();
    if (pid == 0) {
        /* Child */

        /* Close non-adjacent pipe end inherited from parent */
        if (extra_fd >= 0)
            close(extra_fd);

        /* Apply pipe redirections first */
        if (pipe_in >= 0) {
            dup2(pipe_in, 0);
            close(pipe_in);
        }
        if (pipe_out >= 0) {
            dup2(pipe_out, 1);
            close(pipe_out);
        }

        /* Apply file redirections (override pipe if both specified) */
        if (st->in_file) {
            int fd = open(st->in_file, "r");
            if (fd >= 0) {
                dup2(fd, 0);
                close(fd);
            } else {
                print("cannot open ");
                print(st->in_file);
                print("\n");
                exit(1);
            }
        }
        if (st->out_file) {
            int fd = open(st->out_file, st->append ? "a" : "w");
            if (fd >= 0) {
                dup2(fd, 1);
                close(fd);
            } else {
                print("cannot open ");
                print(st->out_file);
                print("\n");
                exit(1);
            }
        }

        /* If builtin, run in child (handles redirected builtins like cd > file) */
        if (is_builtin(st)) {
            run_command(st->argc, st->args);
            exit(0);
        }

        /* External command — build cmdline and exec */
        char cmdline[LINE_MAX];
        int pos = 0;
        for (int i = 0; i < st->argc; i++) {
            if (i > 0) cmdline[pos++] = ' ';
            int j = 0;
            while (st->args[i][j]) cmdline[pos++] = st->args[i][j++];
        }
        cmdline[pos] = '\0';

        /* Restore default signal handling so child responds to Ctrl+C/Z */
        signal(SIGINT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        exit(exec(cmdline));
    }
    return pid;
}

void run_command(int argc, char **args)
{
    if (argc == 0) return;

    /* Builtins — no fork/exec */
    if (strcmp(args[0], "help") == 0) {
        print("Available commands:\n");
        print("  help    - Show this help\n");
        print("  clear   - Clear the screen\n");
        print("  cd      - Change directory\n");
        print("  pwd     - Print working directory\n");
        print("  <prog>  - Run a program\n");
        print("  Pipes:  cmd1 | cmd2\n");
        print("  Redirect: > file, >> file, < file\n");
    } else if (strcmp(args[0], "clear") == 0) {
        print("\x1b\x0F\x0C");
    } else if (strcmp(args[0], "cd") == 0) {
        if (argc < 2) {
            chdir("/");
        } else {
            if (chdir(args[1]) != 0) {
                print("cd: ");
                print(args[1]);
                print(": no such directory\n");
            }
        }
    } else if (strcmp(args[0], "pwd") == 0) {
        char cwd[256];
        if (getcwd(cwd, sizeof(cwd)) == 0)
            print(cwd);
        print("\n");
    } else {
        /* External command — fork+exec */
        int pid = fork();
        if (pid == 0) {
            char cmdline[LINE_MAX];
            int pos = 0;
            for (int i = 0; i < argc; i++) {
                if (i > 0) cmdline[pos++] = ' ';
                int j = 0;
                while (args[i][j]) cmdline[pos++] = args[i][j++];
            }
            cmdline[pos] = '\0';
            signal(SIGINT, SIG_DFL);
            signal(SIGTSTP, SIG_DFL);
            exit(exec(cmdline));
        }
        if (pid > 0) {
            setpgid(pid, pid);
            wait();
            setpgid(0, 0);
        }
    }
}

int main(void)
{
    static char line[LINE_MAX];
    static stage_t stages[MAX_STAGES];

    /* Ignore SIGINT and SIGTSTP — only foreground children should respond */
    signal(SIGINT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);

    print("\x1b\x0F\x0C");
    print("myOS Shell v0.2\n");

    while (1)
    {
        print_prompt();
        if (read_line(line, LINE_MAX) > 0)
        {
            int num_stages = parse_line(line, stages);

            if (num_stages == 1 && stages[0].argc > 0) {
                stage_t *st = &stages[0];

                if (is_builtin(st) && !st->in_file && !st->out_file) {
                    /* Non-redirected builtin — run in parent */
                    run_command(st->argc, st->args);
                } else {
                    /* External command or redirected builtin */
                    int pid = run_stage(st, -1, -1, -1);
                    if (pid > 0) {
                        setpgid(pid, pid);
                        wait();
                        setpgid(0, 0);
                    }
                }
            } else if (num_stages > 1) {
                /* Pipeline: cmd1 | cmd2 | ... | cmdN */
                int prev_fd = -1;
                int child_count = 0;

                for (int i = 0; i < num_stages; i++) {
                    stage_t *st = &stages[i];
                    int pipe_fds[2];
                    int next_fd = -1;
                    int extra_fd = -1;

                    /* Create pipe for all but the last stage */
                    if (i < num_stages - 1) {
                        pipe(pipe_fds);
                        next_fd = pipe_fds[1]; /* write end goes to this stage's stdout */
                        extra_fd = pipe_fds[0]; /* read end is non-adjacent — close in child */
                    }

                    int pid = run_stage(st, prev_fd, next_fd, extra_fd);

                    /* Close pipe ends in parent */
                    if (prev_fd >= 0) close(prev_fd);
                    if (next_fd >= 0) {
                        close(next_fd);
                        prev_fd = pipe_fds[0]; /* read end for next stage */
                    }

                    if (pid > 0) {
                        child_count++;
                        if (child_count == 1)
                            setpgid(pid, pid);
                    }
                }

                /* Wait for successfully created children only */
                for (int i = 0; i < child_count; i++)
                    wait();
                setpgid(0, 0);  /* Shell is foreground again */
            }

            print("\x1b\x0F");
        }
    }
}
