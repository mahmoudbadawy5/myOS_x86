# Development Plan

Three-phase plan to build a hobby x86 OS with userspace utilities.

| Phase | Status | Doc |
|-------|--------|-----|
| Phase 1: Kernel ABI Expansion | DONE | — |
| Phase 2: Minimal ANSI libc | IN PROGRESS | [PHASE2_LIBC.md](PHASE2_LIBC.md) |
| Phase 3: Userspace Utilities | NOT STARTED | [PHASE3_UTILITIES.md](PHASE3_UTILITIES.md) |

---

## Phase 1: Kernel ABI Expansion (DONE)

Expand the kernel to support user-space processes with a proper syscall interface.

### Completed
- [x] GDT user segments (ring 3 code/data)
- [x] Per-process address space (vmm_clone_directory, user page mappings)
- [x] Syscall ABI (regs pointer passed to C, return value written to saved EAX, DPL=3)
- [x] PCB, process table, create_process, schedule, switch_to_process
- [x] Syscalls: test0, test1, open, read, write, exit, yield, sbrk, spawn, wait, exec, dup, pipe, kill, getpid
- [x] Timer-driven preemption scheduler
- [x] Keyboard driver with Ctrl+C signal delivery
- [x] ELF loader with argv support
- [x] Pipe infrastructure
- [x] Process cleanup, orphan prevention
- [x] FAT12 read/write filesystem
- [x] ATA PIO disk driver
- [x] VFS mount points

---

## Phase 2: Minimal ANSI libc (IN PROGRESS)

Build a userspace C library that apps can link against, providing standard headers and functions.

### 2.1 String Functions — `string.h` (partially done)
- [x] strlen
- [ ] strcpy, strncpy
- [ ] strcat, strncat
- [ ] strcmp, strncmp, strcasecmp, strncasecmp
- [ ] strchr, strrchr, strstr
- [ ] memcpy, memmove, memset, memcmp (in kernel, need libc versions)
- [ ] strdup, strtok

### 2.2 Character Classification — `ctype.h`
- [ ] isdigit, isalpha, isalnum, isspace
- [ ] isupper, islower, isprint, iscntrl
- [ ] toupper, tolower

### 2.3 Standard I/O — `stdio.h`
- [x] printf (user-space version via write syscall)
- [ ] fprintf, sprintf, snprintf, vsnprintf
- [ ] puts, putchar, getchar, fgets
- [ ] fopen, fclose, fread, fwrite, fseek, ftell, feof
- [ ] FILE type, stdin/stdout/stderr
- [ ] perror, strerror

### 2.4 Standard Library — `stdlib.h`
- [x] malloc, free (need userspace heap via sbrk)
- [ ] atoi, atol, atof
- [ ] abs, labs
- [ ] exit (wrapper)
- [ ] qsort, bsearch (optional)
- [ ] rand, srand (optional)

### 2.5 Math — `math.h` (optional, low priority)
- [ ] abs, fabs
- [ ] Basic trig/log if needed

### 2.6 File Operations
- [ ] Userspace open, close, read, write, seek (map to syscalls)
- [ ] FILE * wrappers (fopen etc.)

### 2.7 Build System
- [ ] Reorganize libc/ with proper headers (stdio.h, stdlib.h, string.h, ctype.h)
- [ ] Apps link against libc objects
- [ ] Common include path for all apps

---

## Phase 3: Userspace Utilities (NOT STARTED)

Basic shell utilities to make the OS usable.

### Shell Built-ins (already in shell.c)
- [x] cd (builtin)
- [x] exit (builtin)
- [x] echo (spawned externally)

### External Utilities
- [ ] `ls` — list directory contents (readdir)
- [ ] `cat` — print file contents to stdout
- [ ] `mkdir` — create directory (needs kernel syscall)
- [ ] `rm` / `unlink` — delete file (needs kernel syscall)
- [ ] `cp` — copy file
- [ ] `mv` — move/rename file (needs kernel syscall)
- [ ] `pwd` — print working directory
- [ ] `touch` — create empty file
- [ ] `clear` — clear screen
- [ ] `ps` — list processes (needs kernel syscall)
- [ ] `kill` — send signal to process (needs kernel syscall)
- [ ] `head` / `tail` — print first/last lines of file
- [ ] `wc` — word/line/byte count
- [ ] `grep` — basic pattern search
- [ ] `hexdump` / `xxd` — hex view of file

### Kernel Syscalls Needed for Phase 3
- [ ] `mkdir(path, mode)` — create directory
- [ ] `unlink(path)` — delete file
- [ ] `rename(old, new)` — rename file
- [ ] `getcwd(buf, size)` — get current working directory
- [ ] `chdir(path)` — change directory
- [ ] `readdir(fd, dirp, count)` — read directory entries
- [ ] `stat(path, buf)` — get file status
- [ ] `lseek(fd, offset, whence)` — seek in file (may already exist)
- [ ] `getdents(fd, dirp, count)` — get directory entries (Linux-style)

### Nice-to-haves
- [ ] `chmod` / `chown` (permissions — low priority)
- [ ] `mount` / `umount` (user-space mount)
- [ ] `ps` with full process info
- [ ] Environment variables
- [ ] Signal handling (at least SIGINT, SIGTERM)
