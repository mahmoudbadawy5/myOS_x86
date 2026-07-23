#pragma once

unsigned int test_syscall0(unsigned int val);
unsigned int test_syscall1(unsigned int val);
unsigned int print(const char *msg);
unsigned int read(char* buf, int len);
unsigned int exit();
unsigned int sbrk(unsigned int increament);
unsigned int spawn(const char *path);
unsigned int wait();
