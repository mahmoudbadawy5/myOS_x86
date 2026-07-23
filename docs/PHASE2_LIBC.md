# Phase 2: Minimal ANSI libc

## Goal
Build a userspace C library providing standard headers (stdio.h, stdlib.h, string.h, ctype.h) so apps don't need to re-implement basic functions. Apps link against `libc.a` and include standard headers.

## Completed

### Step 1: Headers + Implementations (DONE)
Created 8 headers and 5 source files:
- `stdarg.h`, `stddef.h` — compiler support
- `string.h` / `string.c` — 17 functions: memcpy, memmove, memset, memcmp, strlen, strcpy, strncpy, strcmp, strncmp, strcat, strncat, strcasecmp, strncasecmp, strchr, strrchr, strstr, strdup, strtok
- `ctype.h` / `ctype.c` — 12 functions: isdigit, isalpha, isalnum, isspace, isupper, islower, isprint, iscntrl, isxdigit, toupper, tolower
- `stdio.h` / `stdio.c` — printf, fprintf, sprintf, snprintf, vsnprintf, puts, putchar, getchar, fopen, fclose, fread, fwrite, fgets, fgetc, fputs, fputc, feof, ferror, perror, FILE type, stdin/stdout/stderr
- `stdlib.h` / `stdlib.c` — malloc, free, realloc, atoi, atol, abs, labs
- `syscalls.h` / `syscalls.c` — clean syscall wrappers (sys_open, sys_read, sys_write, sys_read_fd, sys_write_fd, sys_exec, sys_spawn, sys_wait, sys_pipe, sys_dup, sys_kill, sys_getpid, sys_sbrk, sys_exit)
- `test.h` / `test.c` — backward-compatible wrappers (print, read, exit, etc.)

### Step 2: Shell cleanup (DONE)
- Removed local strlen/strcmp/memset from shell.c
- Shell now uses libc functions via test.h includes

## Remaining

### Step 3: lseek syscall (TODO)
- Add `lseek(fd, offset, whence)` to kernel
- Wire up in libc `fseek`/`ftell`

### Step 4: Test app (TODO)
- Write `apps/test_libc.c` exercising:
  - printf formatting (%d, %x, %s, %c)
  - malloc/free
  - string functions
  - ctype functions

### Step 5: Shell modernization (TODO)
- Add `cd` as builtin (tracks cwd)
- Add `clear` as builtin
- Improve command parsing

## Files
```
libc/src/
  include/
    stdarg.h, stddef.h, string.h, ctype.h, stdio.h, stdlib.h, syscalls.h, test.h
  string.c, ctype.c, stdio.c, stdlib.c, syscalls.c, test.c, crt0.asm
```
