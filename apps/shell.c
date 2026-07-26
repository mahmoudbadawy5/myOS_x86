#include <unistd.h>
#include <string.h>
#include <stdio.h>

#define LINE_MAX 256
#define MAX_ARGS 16
#define MAX_STAGES 4
#define MAX_JOBS 16

/* Job states */
#define JOB_RUNNING 1
#define JOB_STOPPED 2
#define JOB_DONE    3

/* Background job tracking */
typedef struct {
    int pid;
    int pgid;
    int status;
    char name[32];
} job_t;

static job_t jobs[MAX_JOBS];
static int num_jobs = 0;

/* A single command stage: args + optional redirections */
typedef struct {
    char *args[MAX_ARGS];
    int argc;
    char *in_file;
    char *out_file;
    int append; /* 1 for >> */
    int background; /* 1 if trailing & */
} stage_t;

static int find_job_by_pid(int pid)
{
    for (int i = 0; i < num_jobs; i++) {
        if (jobs[i].pid == pid)
            return i;
    }
    return -1;
}

static void add_job(int pid, int pgid, const char *name)
{
    if (num_jobs >= MAX_JOBS) return;
    jobs[num_jobs].pid = pid;
    jobs[num_jobs].pgid = pgid;
    jobs[num_jobs].status = JOB_RUNNING;
    int j = 0;
    while (name[j] && j < 31) { jobs[num_jobs].name[j] = name[j]; j++; }
    jobs[num_jobs].name[j] = '\0';
    num_jobs++;
}

static int job_id_of(int index)
{
    int id = 1;
    for (int i = 0; i < index; i++) {
        if (jobs[i].status != JOB_DONE)
            id++;
    }
    return id;
}

static void reap_background_jobs(void)
{
    int pid;
    while ((pid = waitpid(0, WNOHANG)) > 0) {
        int idx = find_job_by_pid(pid);
        if (idx >= 0)
            jobs[idx].status = JOB_DONE;
    }
}

static void compact_jobs(void)
{
    int dst = 0;
    for (int src = 0; src < num_jobs; src++) {
        if (jobs[src].status != JOB_DONE) {
            if (dst != src)
                jobs[dst] = jobs[src];
            dst++;
        }
    }
    num_jobs = dst;
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
                printf("\n");
                return i;
            }
            else if (c == '\b' || c == 0x7F)
            {
                if (i > 0)
                {
                    i--;
                    printf("\b \b");
                }
            }
            else
            {
                printf("%c", c);
                buf[i++] = c;
            }
        }
    }
    buf[i] = '\0';
    return i;
}

int parse_line(char *line, stage_t *stages)
{
    int num_stages = 0;
    char *p = line;

    while (*p == ' ') p++;
    if (*p == '\0') return 0;

    while (*p && num_stages < MAX_STAGES) {
        stage_t *st = &stages[num_stages];
        st->argc = 0;
        st->in_file = 0;
        st->out_file = 0;
        st->append = 0;
        st->background = 0;

        while (*p && *p != '|') {
            while (*p == ' ') p++;
            if (*p == '\0' || *p == '|') break;

            if (*p == '<') {
                p++;
                while (*p == ' ') p++;
                st->in_file = p;
                while (*p && *p != ' ' && *p != '|' && *p != '>' && *p != '<') p++;
                if (*p) { *p = '\0'; p++; }
            } else if (*p == '>') {
                p++;
                if (*p == '>') { st->append = 1; p++; }
                while (*p == ' ') p++;
                st->out_file = p;
                while (*p && *p != ' ' && *p != '|' && *p != '>' && *p != '<') p++;
                if (*p) { *p = '\0'; p++; }
            } else {
                if (st->argc < MAX_ARGS - 1)
                    st->args[st->argc++] = p;
                while (*p && *p != ' ' && *p != '|' && *p != '>' && *p != '<') p++;
                if (*p) { *p = '\0'; p++; }
            }
        }
        st->args[st->argc] = 0;

        if (st->argc > 0)
            num_stages++;

        if (*p == '|') {
            p++;
            while (*p == ' ') p++;
        }
    }

    /* Check for trailing & on the last stage */
    if (num_stages > 0) {
        stage_t *last = &stages[num_stages - 1];
        if (last->argc > 0) {
            char *last_arg = last->args[last->argc - 1];
            if (strcmp(last_arg, "&") == 0) {
                last->background = 1;
                last->argc--;
                last->args[last->argc] = 0;
            }
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
            strcmp(st->args[0], "pwd") == 0 ||
            strcmp(st->args[0], "jobs") == 0 ||
            strcmp(st->args[0], "fg") == 0 ||
            strcmp(st->args[0], "bg") == 0);
}

int run_stage(stage_t *st, int pipe_in, int pipe_out, int extra_fd)
{
    int pid = fork();
    if (pid == 0) {
        if (extra_fd >= 0)
            close(extra_fd);

        if (pipe_in >= 0) {
            dup2(pipe_in, 0);
            close(pipe_in);
        }
        if (pipe_out >= 0) {
            dup2(pipe_out, 1);
            close(pipe_out);
        }

        if (st->in_file) {
            int fd = open(st->in_file, "r");
            if (fd >= 0) {
                dup2(fd, 0);
                close(fd);
            } else {
                printf("cannot open %s\n", st->in_file);
                exit(1);
            }
        }
        if (st->out_file) {
            int fd = open(st->out_file, st->append ? "a" : "w");
            if (fd >= 0) {
                dup2(fd, 1);
                close(fd);
            } else {
                printf("cannot open %s\n", st->out_file);
                exit(1);
            }
        }

        if (is_builtin(st)) {
            run_command(st->argc, st->args);
            exit(0);
        }

        char cmdline[LINE_MAX];
        int pos = 0;
        for (int i = 0; i < st->argc; i++) {
            if (i > 0) cmdline[pos++] = ' ';
            int j = 0;
            while (st->args[i][j]) cmdline[pos++] = st->args[i][j++];
        }
        cmdline[pos] = '\0';

        signal(SIGINT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        exit(exec(cmdline));
    }
    return pid;
}

void run_command(int argc, char **args)
{
    if (argc == 0) return;

    if (strcmp(args[0], "help") == 0) {
        printf("Available commands:\n");
        printf("  help       - Show this help\n");
        printf("  clear      - Clear the screen\n");
        printf("  cd         - Change directory\n");
        printf("  pwd        - Print working directory\n");
        printf("  jobs       - List background jobs\n");
        printf("  fg [N]     - Bring job N to foreground\n");
        printf("  bg [N]     - Resume stopped job N in background\n");
        printf("  <prog>     - Run a program\n");
        printf("  <prog> &   - Run in background\n");
        printf("  Pipes:     cmd1 | cmd2\n");
        printf("  Redirect:  > file, >> file, < file\n");
    } else if (strcmp(args[0], "clear") == 0) {
        printf("\x1b\x0F\x0C");
    } else if (strcmp(args[0], "cd") == 0) {
        if (argc < 2) {
            chdir("/");
        } else {
            if (chdir(args[1]) != 0)
                printf("cd: %s: no such directory\n", args[1]);
        }
    } else if (strcmp(args[0], "pwd") == 0) {
        char cwd[256];
        if (getcwd(cwd, sizeof(cwd)) == 0)
            printf("%s\n", cwd);
    } else if (strcmp(args[0], "jobs") == 0) {
        reap_background_jobs();
        compact_jobs();
        if (num_jobs == 0) {
            printf("No jobs\n");
        } else {
            for (int i = 0; i < num_jobs; i++) {
                const char *st_str = "Running";
                if (jobs[i].status == JOB_STOPPED) st_str = "Stopped";
                else if (jobs[i].status == JOB_DONE) st_str = "Done";
                printf("[%d]  %d  %s  %s\n", job_id_of(i), jobs[i].pid, st_str, jobs[i].name);
            }
        }
    } else if (strcmp(args[0], "fg") == 0) {
        reap_background_jobs();
        compact_jobs();
        int target = -1;
        if (argc >= 2) {
            int id = 0;
            for (int i = 0; args[1][i]; i++)
                id = id * 10 + (args[1][i] - '0');
            for (int i = 0; i < num_jobs; i++) {
                if (jobs[i].status != JOB_DONE && job_id_of(i) == id) {
                    target = i;
                    break;
                }
            }
        } else {
            for (int i = num_jobs - 1; i >= 0; i--) {
                if (jobs[i].status == JOB_STOPPED || jobs[i].status == JOB_RUNNING) {
                    target = i;
                    break;
                }
            }
        }
        if (target < 0) {
            printf("fg: no such job\n");
            return;
        }
        setpgid(jobs[target].pid, jobs[target].pid);
        if (jobs[target].status == JOB_STOPPED)
            kill(jobs[target].pid, SIGCONT);
        waitpid(jobs[target].pid, 0);
        if (kill(jobs[target].pid, 0) == 0) {
            /* Still alive — stopped again */
            jobs[target].status = JOB_STOPPED;
            printf("\n[%d]+ Stopped  %s\n", job_id_of(target), jobs[target].name);
        } else {
            /* Terminated — remove from jobs */
            for (int i = target; i < num_jobs - 1; i++)
                jobs[i] = jobs[i + 1];
            num_jobs--;
        }
        setpgid(0, 0);
    } else if (strcmp(args[0], "bg") == 0) {
        reap_background_jobs();
        compact_jobs();
        int target = -1;
        if (argc >= 2) {
            int id = 0;
            for (int i = 0; args[1][i]; i++)
                id = id * 10 + (args[1][i] - '0');
            for (int i = 0; i < num_jobs; i++) {
                if (jobs[i].status != JOB_DONE && job_id_of(i) == id) {
                    target = i;
                    break;
                }
            }
        } else {
            for (int i = num_jobs - 1; i >= 0; i--) {
                if (jobs[i].status == JOB_STOPPED) {
                    target = i;
                    break;
                }
            }
        }
        if (target < 0) {
            printf("bg: no such stopped job\n");
            return;
        }
        jobs[target].status = JOB_RUNNING;
        kill(jobs[target].pid, SIGCONT);
        printf("[%d] %s &\n", job_id_of(target), jobs[target].name);
    } else {
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

    signal(SIGINT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);

    printf("\x1b\x0F\x0C");
    printf("myOS Shell v0.3\n");

    while (1)
    {
        reap_background_jobs();

        printf("\x1b\x0FmyOS> ");
        if (read_line(line, LINE_MAX) > 0)
        {
            int num_stages = parse_line(line, stages);

            if (num_stages == 1 && stages[0].argc > 0) {
                stage_t *st = &stages[0];

                if (is_builtin(st) && !st->in_file && !st->out_file && !st->background) {
                    run_command(st->argc, st->args);
                } else {
                    int pid = run_stage(st, -1, -1, -1);
                    if (pid > 0) {
                        if (st->background) {
                            add_job(pid, pid, st->args[0]);
                            setpgid(pid, pid);
                            printf("[%d] %d\n", num_jobs, pid);
                        } else {
                            add_job(pid, pid, st->args[0]);
                            setpgid(pid, pid);
                            wait();
                            if (kill(pid, 0) == 0) {
                                /* child still alive → stopped */
                                int idx = find_job_by_pid(pid);
                                if (idx >= 0) {
                                    jobs[idx].status = JOB_STOPPED;
                                    printf("\n[%d]+ Stopped  %s\n", job_id_of(idx), jobs[idx].name);
                                }
                            } else {
                                /* child dead → terminated */
                                int idx = find_job_by_pid(pid);
                                if (idx >= 0)
                                    jobs[idx].status = JOB_DONE;
                            }
                            setpgid(0, 0);
                        }
                    }
                }
            } else if (num_stages > 1) {
                int prev_fd = -1;
                int child_count = 0;
                int last_pid = -1;
                int background = stages[num_stages - 1].background;

                for (int i = 0; i < num_stages; i++) {
                    stage_t *st = &stages[i];
                    int pipe_fds[2];
                    int next_fd = -1;
                    int extra_fd = -1;

                    if (i < num_stages - 1) {
                        pipe(pipe_fds);
                        next_fd = pipe_fds[1];
                        extra_fd = pipe_fds[0];
                    }

                    int pid = run_stage(st, prev_fd, next_fd, extra_fd);

                    if (prev_fd >= 0) close(prev_fd);
                    if (next_fd >= 0) {
                        close(next_fd);
                        prev_fd = pipe_fds[0];
                    }

                    if (pid > 0) {
                        child_count++;
                        last_pid = pid;
                        if (child_count == 1)
                            setpgid(pid, pid);
                    }
                }

                if (background) {
                    add_job(last_pid, last_pid, stages[0].args[0]);
                } else {
                    add_job(last_pid, last_pid, stages[0].args[0]);
                    for (int i = 0; i < child_count; i++)
                        wait();
                    if (kill(last_pid, 0) == 0) {
                        int idx = find_job_by_pid(last_pid);
                        if (idx >= 0) {
                            jobs[idx].status = JOB_STOPPED;
                            printf("\n[%d]+ Stopped  %s\n", job_id_of(idx), jobs[idx].name);
                        }
                    } else {
                        int idx = find_job_by_pid(last_pid);
                        if (idx >= 0)
                            jobs[idx].status = JOB_DONE;
                    }
                    setpgid(0, 0);
                }
            }

            printf("\x1b\x0F");
        }
    }
}
