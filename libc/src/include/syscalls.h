#ifndef _SYSCALLS_H
#define _SYSCALLS_H

int sys_test0(int val);
int sys_test1(int val);
int sys_open(const char *path, const char *mode);
int sys_read(void *buf, int len);
int sys_read_fd(int fd, void *buf, int len);
int sys_write(const char *buf, int len);
int sys_write_fd(int fd, const char *buf, int len);
void sys_exit(void);
void *sys_sbrk(unsigned int increment);
int sys_spawn(const char *path);
int sys_wait(void);
int sys_exec(const char *cmdline);
int sys_dup(int fd);
int sys_pipe(int fds[2]);
int sys_kill(int pid, int signal);
int sys_getpid(void);
int sys_lseek(int fd, int offset, int whence);
int sys_readdir(const char *path, char *buf, int max_entries);
int sys_stat(const char *path, unsigned int *stat_buf);
int sys_getcwd(char *buf, unsigned int size);
int sys_chdir(const char *path);

#endif
