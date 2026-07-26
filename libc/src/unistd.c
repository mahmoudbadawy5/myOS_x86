#include <unistd.h>
#include <string.h>

/* Process */
int fork(void)              { return sys_fork(); }
void exit(int status)       { sys_exit(status); for(;;); }
int wait(void)              { return sys_wait(); }
int exec(const char *cmd)   { return sys_exec(cmd); }
unsigned int spawn(const char *path) { return sys_spawn(path); }
int getpid(void)            { return sys_getpid(); }
int kill(int pid, int sig)  { return sys_kill(pid, sig); }
void yield(void)            { sys_yield(); }

/* File descriptors */
int open(const char *path, const char *mode) { return sys_open(path, mode); }
int close(int fd)           { return sys_close(fd); }
int read(int fd, void *buf, int len)  { return sys_read_fd(fd, buf, len); }
int write(int fd, const void *buf, int len) { return sys_write_fd(fd, buf, len); }
int lseek(int fd, int offset, int whence) { return sys_lseek(fd, offset, whence); }
int dup(int fd)             { return sys_dup(fd); }
int dup2(int oldfd, int newfd) { return sys_dup2(oldfd, newfd); }

/* Pipes */
int pipe(int fds[2])        { return sys_pipe(fds); }

/* Filesystem */
int readdir(const char *path, char *buf, int max) { return sys_readdir(path, buf, max); }
int stat(const char *path, unsigned int *buf) { return sys_stat(path, buf); }
int getcwd(char *buf, unsigned int size) { return sys_getcwd(buf, size); }
int chdir(const char *path) { return sys_chdir(path); }
int mkdir(const char *path) { return sys_mkdir(path); }
int unlink(const char *path) { return sys_unlink(path); }

/* Memory */
void *mmap(void *addr, unsigned int length, int flags, int fd, int offset) {
    return sys_mmap(addr, length, flags, fd, offset);
}
int munmap(void *addr, unsigned int length) { return sys_munmap(addr, length); }

/* Misc */
unsigned int print(const char *msg) { return sys_write(msg, strlen(msg)); }
int ps(void *buf, int max) { return sys_ps(buf, max); }
void *sbrk(unsigned int inc) { return sys_sbrk(inc); }

/* Kernel test syscalls */
unsigned int test_syscall0(unsigned int val) { return sys_test0(val); }
unsigned int test_syscall1(unsigned int val) { return sys_test1(val); }
