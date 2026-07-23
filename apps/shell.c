#include <test.h>

#define LINE_MAX 256
#define MAX_ARGS 16

static int strlen(const char *s)
{
    int i = 0;
    while (s[i])
        i++;
    return i;
}

static int strcmp(const char *a, const char *b)
{
    while (*a && *a == *b)
    {
        a++;
        b++;
    }
    return *a - *b;
}

static void memset(char *buf, char val, int len)
{
    for (int i = 0; i < len; i++)
        buf[i] = val;
}

void print_prompt(void)
{
    print("myOS> ");
}

int read_line(char *buf, int max)
{
    int i = 0;
    char c;
    while (i < max - 1)
    {
        if (read(&c, 1))
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

int parse_args(char *line, char **args)
{
    int argc = 0;
    int in_word = 0;
    for (int i = 0; line[i] && argc < MAX_ARGS - 1; i++)
    {
        char c = line[i];
        if (c == ' ' || c == '\t')
        {
            if (in_word)
            {
                line[i] = '\0';
                in_word = 0;
            }
        }
        else
        {
            if (!in_word)
            {
                args[argc++] = &line[i];
                in_word = 1;
            }
        }
    }
    args[argc] = 0;
    return argc;
}

void run_command(int argc, char **args)
{
    if (argc == 0)
        return;

    if (strcmp(args[0], "help") == 0)
    {
        print("Available commands:\n");
        print("  help    - Show this help\n");
        print("  clear   - Clear the screen\n");
        print("  <prog>  - Run a program\n");
    }
    else if (strcmp(args[0], "clear") == 0)
    {
        print("\x1b\x40");
    }
    else
    {
        /* Reconstruct full command line from args (parse_args nulled the spaces) */
        char cmdline[LINE_MAX];
        int pos = 0;
        for (int i = 0; i < argc; i++) {
            if (i > 0) cmdline[pos++] = ' ';
            int j = 0;
            while (args[i][j]) cmdline[pos++] = args[i][j++];
        }
        cmdline[pos] = '\0';

        print("Spawning: ");
        print(args[0]);
        print("\n");
        spawn(cmdline);
        wait();
    }
}

int main(void)
{
    static char line[LINE_MAX];
    static char *args[MAX_ARGS];

    print("\x1b\x40");
    print("myOS Shell v0.1\n");

    while (1)
    {
        print_prompt();
        if (read_line(line, LINE_MAX) > 0)
        {
            int argc = parse_args(line, args);
            if (argc > 0)
                run_command(argc, args);
        }
    }
}
