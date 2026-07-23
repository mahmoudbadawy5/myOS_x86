# Phase 2: Minimal ANSI libc

## Goal
Build a userspace C library providing standard headers (stdio.h, stdlib.h, string.h, ctype.h) so apps don't need to re-implement basic functions. Apps link against `libc.a` and include standard headers.

## Completed

### Step 1: Headers + Implementations (DONE)
Created 8 headers and 6 source files:
- `stdarg.h`, `stddef.h` -- compiler support
- `string.h` / `string.c` -- 18 functions: memcpy, memmove, memset, memcmp, strlen, strcpy, strncpy, strcmp, strncmp, strcat, strncat, strcasecmp, strncasecmp, strchr, strrchr, strstr, strdup, strtok
- `ctype.h` / `ctype.c` -- 12 functions: isdigit, isalpha, isalnum, isspace, isupper, islower, isprint, iscntrl, isxdigit, toupper, tolower
- `stdio.h` / `stdio.c` -- printf, fprintf, sprintf, snprintf, vsnprintf, puts, putchar, getchar, fopen, fclose, fread, fwrite, fgets, fgetc, fputs, fputc, feof, ferror, perror, FILE type, stdin/stdout/stderr
- `stdlib.h` / `stdlib.c` -- malloc, free, realloc, atoi, atol, abs, labs
- `syscalls.h` / `syscalls.c` -- clean syscall wrappers (sys_open, sys_read, sys_write, sys_read_fd, sys_write_fd, sys_exec, sys_spawn, sys_wait, sys_pipe, sys_dup, sys_kill, sys_getpid, sys_sbrk, sys_lseek, sys_exit)
- `test.h` / `test.c` -- backward-compatible wrappers (print, read, exit, etc.)

### Step 2: Shell cleanup (DONE)
- Removed local strlen/strcmp/memset from shell.c
- Shell now uses libc functions via test.h includes

### Step 3: lseek syscall (DONE)
- Added `sys_lseek` (syscall 15) in kernel (`kernel/src/arch/syscalls.c`)
- Wired up `fseek` and `ftell` in libc (`libc/src/stdio.c`)

### Step 4: Test app (DONE)
- `apps/test_libc.c` exercises:
  - string.h: strlen, strcpy, strcmp, strncmp, strcat, memcpy, memset, strstr
  - ctype.h: isdigit, isalpha, isspace, toupper, tolower
  - stdlib.h: atoi, malloc/free, realloc
  - printf formats: %d, %x, %o, %s, %c, %p, %08x

### Bug fixes (DONE)
- Fixed page fault (EIP=0x00000001) in syscall_sbrk: `switch_to_process` in `process_asm.asm` now updates `cur_page_dir` after setting CR3
- Removed redundant `set_page_dir` save/restore in `syscall_sbrk` and `syscall_pipe`
- Fixed `print_uint` zero-padding bug: leading zeros now use a separate counter instead of corrupting the digit count

## Remaining

### Step 5: Shell modernization (TODO)
- Add `cd` as builtin (tracks cwd)
- Add `clear` as builtin
- Improve command parsing

## Files
```
libc/
  src/include/
    stdarg.h, stddef.h, string.h, ctype.h, stdio.h, stdlib.h, syscalls.h, test.h
  src/
    string.c, ctype.c, stdio.c, stdlib.c, syscalls.c, test.c, crt0.asm
kernel/src/arch/syscalls.c  (lseek syscall)
kernel/src/proc/process_asm.asm  (cur_page_dir sync fix)
apps/test_libc.c
```
