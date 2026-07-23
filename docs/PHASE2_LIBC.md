# Phase 2: Minimal ANSI libc

## Goal
Build a userspace C library providing standard headers (stdio.h, stdlib.h, string.h, ctype.h) so apps don't need to re-implement basic functions. Apps link against `libc.a` and include standard headers.

## Current State
- `libc/src/test.c` — raw syscall wrappers (print, read, exit, sbrk, spawn, wait, exec, dup, pipe, kill, getpid)
- `libc/src/include/test.h` — declarations for above
- `libc/src/crt0.asm` — C runtime startup (_start)
- `libc/Makefile` — builds `build/libc.o` (relocatable object linked into apps)
- Each app re-implements strlen, strcmp, memset locally (see shell.c)
- No standard headers exist

## What Apps Need (survey of shell.c, echo.c, etc.)
- String: strlen, strcpy, strcmp, strcat, memset, memcpy
- Stdio: printf, puts, putchar (via write syscall)
- Stdlib: malloc/free (via sbrk syscall)
- Ctype: toupper, tolower, isdigit, isspace (for arg parsing)

## Implementation Plan

### Step 1: Create proper header structure
```
libc/src/include/
  stdio.h     — FILE, printf, fprintf, sprintf, puts, putchar, fgets, fopen, fclose, fread, fwrite
  stdlib.h    — malloc, free, atoi, atol, exit, abs
  string.h    — memcpy, memset, memcmp, memmove, strlen, strcpy, strncpy, strcmp, strncmp, strcat, strncat, strchr, strrchr, strstr, strdup
  ctype.h     — isdigit, isalpha, isalnum, isspace, isupper, islower, toupper, tolower, isprint, iscntrl
  test.h      — raw syscall wrappers (keep as-is, internal)
```

### Step 2: Implement string.h functions
Source: `libc/src/string.c`
- memcpy, memset, memcmp, memmove (from kernel, port to libc)
- strlen, strcpy, strcmp (from kernel, port)
- strncpy, strncmp, strcasecmp, strncasecmp
- strcat, strncat
- strchr, strrchr, strstr
- strdup, strtok

### Step 3: Implement ctype.h functions
Source: `libc/src/ctype.c`
- Inline functions or macro-based character classification tables
- isdigit, isalpha, isalnum, isspace, isupper, islower, isprint, iscntrl, toupper, tolower

### Step 4: Implement stdio.h — formatted I/O
Source: `libc/src/stdio.c`
- vsnprintf (port from kernel's print.c, remove kernel dependencies)
- printf, fprintf, sprintf, snprintf (wrappers around vsnprintf)
- puts(s) — write(s, strlen(s), 1) + write("\n", 1, 1)
- putchar(c) — write(&c, 1, 1)
- fgets(buf, size, fd) — read character by character

### Step 5: Implement stdio.h — file operations
Source: `libc/src/fio.c`
- FILE struct: { int fd; int flags; int eof; int error; }
- fopen(path, mode) — open(path, ...) syscall, allocate FILE
- fclose(file) — close(file->fd), free FILE
- fread(buf, size, count, file) — read(file->fd, buf, size*count)
- fwrite(buf, size, count, file) — write(file->fd, buf, size*count)
- fseek(file, offset, whence) — needs lseek syscall (ADD IF MISSING)
- feof, ferror

### Step 6: Implement stdlib.h — memory
Source: `libc/src/stdlib.c`
- malloc/free — userspace heap manager using sbrk syscall
  - Simple first-fit or best-fit allocator
  - malloc_header_t: { size_t size; int free; malloc_header_t *next; }
  - free coalesces adjacent blocks
- atoi(s) — parse integer string
- atol(s) — parse long
- abs(n) — absolute value

### Step 7: Add lseek syscall (if missing)
- Kernel side: syscall_lseek in syscalls.c + syscalls_asm.asm entry
- Updates file's seek_offset

### Step 8: Update Makefile and app builds
- Update `apps/Makefile` to include all libc .o files
- Remove duplicate strlen/strcmp/memset from individual apps
- Update app includes from `<test.h>` to `<stdio.h>` / `<string.h>` / etc.

### Step 9: Test
- Update shell to use new libc (remove local strlen, strcmp, memset)
- Add a simple test app that exercises printf, malloc, fopen, etc.

## Files to Create/Modify
- `libc/src/include/stdio.h` (NEW)
- `libc/src/include/stdlib.h` (NEW)
- `libc/src/include/string.h` (NEW — replaces kernel-style header)
- `libc/src/include/ctype.h` (NEW)
- `libc/src/string.c` (NEW — full string functions)
- `libc/src/ctype.c` (NEW)
- `libc/src/stdio.c` (NEW — printf family + file ops)
- `libc/src/stdlib.c` (NEW — malloc/free + atoi etc.)
- `libc/src/test.c` (MODIFY — keep syscall wrappers, remove strlen)
- `libc/Makefile` (MODIFY)
- `apps/Makefile` (MODIFY)
- `apps/shell.c` (MODIFY — use libc headers)
- `kernel/src/arch/syscalls.c` (ADD lseek)
- `kernel/src/arch/syscalls_asm.asm` (ADD lseek entry)

## Order of Implementation
1. string.h + string.c (foundation — everything depends on it)
2. ctype.h + ctype.c (simple, no deps)
3. stdio.h + stdio.c (printf family — needs string.h)
4. stdlib.h + stdlib.c (malloc/free — needs sbrk, string.h)
5. fio.c (file ops — needs open/close/read/write syscalls)
6. lseek syscall (if needed)
7. Update shell.c to use new libc
8. Test
