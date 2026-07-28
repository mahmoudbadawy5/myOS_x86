#ifndef _UNISTD_H
#define _UNISTD_H

#include <syscalls.h>

/* Process */
int fork(void);
void exit(int status);
int wait(void);
int exec(const char *cmdline);
unsigned int spawn(const char *path);
int getpid(void);
int kill(int pid, int signal);
void yield(void);

/* Signals */
#define NSIG       32
#define SIG_DFL    ((sighandler_t)0)
#define SIG_IGN    ((sighandler_t)1)
#define SIGINT     2
#define SIGKILL    9
#define SIGSTOP    17
#define SIGCONT    18
#define SIGCHLD    19
#define SIGTSTP    20
#define SIG_BIT(s) (1U << (s))

typedef void (*sighandler_t)(int signum);
sighandler_t signal(int signum, sighandler_t handler);
int sigreturn(void);

/* Signal masks */
typedef unsigned int sigset_t;
#define SIG_BLOCK    0  /* Block signals in set */
#define SIG_UNBLOCK  1  /* Unblock signals in set */
#define SIG_SETMASK  2  /* Set mask to exactly set */
int sigprocmask(int how, const sigset_t *set, sigset_t *oldset);

int setpgid(int pid, int pgid);
int waitpid(int pid, int options);
#define WNOHANG 1

/* File descriptors */
int open(const char *path, const char *mode);
int close(int fd);
int read(int fd, void *buf, int len);
int write(int fd, const void *buf, int len);
int lseek(int fd, int offset, int whence);
int dup(int fd);
int dup2(int oldfd, int newfd);

/* Pipes */
int pipe(int fds[2]);

/* Filesystem */
int readdir(const char *path, char *buf, int max_entries);
int stat(const char *path, unsigned int *stat_buf);
int getcwd(char *buf, unsigned int size);
int chdir(const char *path);
int mkdir(const char *path);
int unlink(const char *path);

/* Memory */
void *mmap(void *addr, unsigned int length, int flags, int fd, int offset);
int munmap(void *addr, unsigned int length);

/* Misc */
unsigned int print(const char *msg);
int ps(void *buf, int max_entries);
void *sbrk(unsigned int increment);

/* Kernel test syscalls */
unsigned int test_syscall0(unsigned int val);
unsigned int test_syscall1(unsigned int val);

/* Graphics */
int fb_set_mode(int w, int h, int bpp);
void *fb_map(void);
int fb_restore_text(void);

/* Sleep */
unsigned int sleep(unsigned int seconds);

#endif
