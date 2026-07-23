# Development Plan

Three-phase plan to build a hobby x86 OS with userspace utilities.

| Phase | Status | Doc |
|-------|--------|-----|
| Phase 1: Kernel ABI Expansion | DONE | — |
| Phase 2: Minimal ANSI libc | DONE | [PHASE2_LIBC.md](PHASE2_LIBC.md) |
| Phase 3: Userspace Utilities | IN PROGRESS | [PHASE3_UTILITIES.md](PHASE3_UTILITIES.md) |
| Phase 4: Ring 3 Process Hardening | NOT STARTED | [RING3_INFRA.md](RING3_INFRA.md) |

---

## Phase 1: Kernel ABI Expansion (DONE)

Expand the kernel to support user-space processes with a proper syscall interface.

### Completed
- [x] GDT user segments (ring 3 code/data)
- [x] Per-process address space (vmm_clone_directory, user page mappings)
- [x] Syscall ABI (regs pointer passed to C, return value written to saved EAX, DPL=3)
- [x] PCB, process table, create_process, schedule, switch_to_process
- [x] Syscalls: test0, test1, open, read, write, exit, yield, sbrk, spawn, wait, exec, dup, pipe, kill, getpid, lseek, readdir, stat, getcwd, chdir
- [x] Timer-driven preemption scheduler
- [x] Keyboard driver with Ctrl+C signal delivery
- [x] ELF loader with argv support
- [x] Pipe infrastructure
- [x] Process cleanup, orphan prevention
- [x] FAT12 read/write filesystem
- [x] ATA PIO disk driver
- [x] VFS mount points

---

## Phase 2: Minimal ANSI libc (DONE)

Build a userspace C library that apps can link against, providing standard headers and functions.

### Completed
- [x] string.h: memcpy, memmove, memset, memcmp, strlen, strcpy, strncpy, strcat, strncat, strcmp, strncmp, strcasecmp, strncasecmp, strchr, strrchr, strstr, strdup, strtok
- [x] ctype.h: isdigit, isalpha, isalnum, isspace, isupper, islower, isprint, iscntrl, isxdigit, toupper, tolower
- [x] stdio.h: printf, fprintf, sprintf, snprintf, vsnprintf, puts, putchar, getchar, fopen, fclose, fread, fwrite, fseek, ftell, feof, ferror, fgets, fgetc, fputs, fputc, perror
- [x] stdlib.h: malloc, free, realloc, atoi, atol, abs, labs
- [x] Syscall wrappers: sys_open, sys_read, sys_read_fd, sys_write, sys_write_fd, sys_exit, sys_sbrk, sys_spawn, sys_wait, sys_exec, sys_dup, sys_pipe, sys_kill, sys_getpid, sys_lseek, sys_readdir, sys_stat, sys_getcwd, sys_chdir

### Remaining (Phase 3 scope)
- [ ] `close` syscall + `sys_close` wrapper (needed for `fclose` to release fd)
- [ ] Standard `void exit(int)` signature (current `exit` in test.c has wrong signature)
- [ ] `strerror`, `errno` (nice to have, not blocking)

---

## Phase 3: Userspace Utilities (IN PROGRESS)

### 3A — `close` syscall (syscall #20)

Unblocks proper `fclose`. Currently `fclose` only frees the FILE struct but leaks the fd.

| File | Change |
|------|--------|
| `kernel/src/arch/syscalls.c` | Add `syscall_close` handler — bounds-check fd, free `files_open[fd]`, set to NULL |
| `kernel/src/arch/syscalls.c` | Add `syscall_close` to table at index 20 |
| `kernel/src/arch/syscalls_asm.asm` | Bump `MAX_SYSCALL` from 19 to 20 |
| `kernel/src/include/arch/syscalls.h` | Bump `MAX_SYSCALLS` from 20 to 21 |
| `libc/src/syscalls.c` | Add `sys_close(int fd)` wrapper |
| `libc/src/include/syscalls.h` | Declare `sys_close` |
| `libc/src/stdio.c` | Update `fclose` to call `sys_close(fp->fd)` |

### 3B — `mkdir` syscall (syscall #21) + app

| File | Change |
|------|--------|
| `kernel/src/include/fs/vfs.h` | Add `mkdir_type_t` + `mkdir` field on `fs_node_t` |
| `kernel/src/fs/vfs.c` | Add `mkdir_fs(parent, name)` wrapper |
| `kernel/src/fs/fat12.c` | Add `fat12_mkdir()` — allocate cluster, init `.`/`..` entries (attr 0x10), create dir entry |
| `kernel/src/fs/fat12.c` | Wire `fat12_root.mkdir = fat12_mkdir` in `init_fat12` |
| `kernel/src/arch/syscalls.c` | Add `syscall_mkdir` — resolve path, find parent, call `mkdir_fs` |
| `kernel/src/arch/syscalls.c` | Add to table at index 21 |
| `kernel/src/arch/syscalls_asm.asm` | Bump `MAX_SYSCALL` to 21 |
| `kernel/src/include/arch/syscalls.h` | Bump `MAX_SYSCALLS` to 22 |
| `libc/src/syscalls.c` | Add `sys_mkdir(const char *path)` |
| `libc/src/include/syscalls.h` | Declare `sys_mkdir` |
| `apps/mkdir.c` | New app — `mkdir <path>` |
| `Makefile` | Add `mkdir.bin` target |

### 3C — `unlink`/`rm` syscall (syscall #22) + app

`fat12_free_cluster_chain()` already exists but is unused — wire it up.

| File | Change |
|------|--------|
| `kernel/src/include/fs/vfs.h` | Add `unlink_type_t` + `unlink` field on `fs_node_t` |
| `kernel/src/fs/vfs.c` | Add `unlink_fs(parent, name)` wrapper |
| `kernel/src/fs/fat12.c` | Add `fat12_unlink()` — mark entry deleted (0xE5), free cluster chain, flush |
| `kernel/src/fs/fat12.c` | Wire `fat12_root.unlink = fat12_unlink` |
| `kernel/src/arch/syscalls.c` | Add `syscall_unlink` — resolve path, split parent/name, call `unlink_fs` |
| `kernel/src/arch/syscalls.c` | Add to table at index 22 |
| `kernel/src/arch/syscalls_asm.asm` | Bump `MAX_SYSCALL` to 22 |
| `kernel/src/include/arch/syscalls.h` | Bump `MAX_SYSCALLS` to 23 |
| `libc/src/syscalls.c` | Add `sys_unlink(const char *path)` |
| `libc/src/include/syscalls.h` | Declare `sys_unlink` |
| `apps/rm.c` | New app — `rm <file>` |
| `Makefile` | Add `rm.bin` target |

### 3D — `ps` syscall (syscall #23) + app

| File | Change |
|------|--------|
| `kernel/src/arch/syscalls.c` | Add `syscall_ps` — iterate `process_table`, copy pid/state/name/parent_id to user buffer |
| `kernel/src/arch/syscalls.c` | Add to table at index 23 |
| `kernel/src/arch/syscalls_asm.asm` | Bump `MAX_SYSCALL` to 23 |
| `kernel/src/include/arch/syscalls.h` | Bump `MAX_SYSCALLS` to 24 |
| `libc/src/syscalls.c` | Add `sys_ps(void *buf, int max)` |
| `libc/src/include/syscalls.h` | Declare `sys_ps` + `ps_entry_t` |
| `apps/ps.c` | New app — `ps` |
| `Makefile` | Add `ps.bin` target |

### 3E — Text utilities (no new syscalls)

All use existing open/read/write from libc.

| App | Description |
|-----|-------------|
| `apps/head.c` | Print first N lines (default 10), `-n N` flag |
| `apps/tail.c` | Print last N lines (default 10), `-n N` flag |
| `apps/wc.c` | Count lines/words/bytes |
| `apps/grep.c` | Basic string search — read line by line, `strstr()` match |
| `apps/hexdump.c` | Hex view — read bytes, print hex+ASCII |

Each needs a Makefile target and inclusion in `tools/create_initrd.py`.

### Execution order

1. Part A — `close` syscall
2. Part B — `mkdir`
3. Part C — `unlink`/`rm`
4. Part D — `ps`
5. Part E — head, tail, wc, grep, hexdump
6. Build, test all apps in QEMU
7. Commit

---

## Phase 4: Ring 3 Process Hardening (NOT STARTED)

See [RING3_INFRA.md](RING3_INFRA.md) for full current-state audit and planned work.

### Scope (to be decided after Phase 3)
- User-mode page fault handling (SIGSEGV instead of kernel panic)
- fork() support (copy-on-write or full page copy)
- Process priority/nice
