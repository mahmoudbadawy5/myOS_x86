# Phase 3: Userspace Utilities

## Goal
Basic shell utilities to make the OS usable — ls, cat, mkdir, rm, etc.

## Prerequisites
- Phase 2 complete (proper libc with stdio.h, stdlib.h, string.h, ctype.h)
- Kernel syscalls for filesystem ops (mkdir, unlink, readdir, etc.)

## New Kernel Syscalls Needed
| Syscall | Nr | Args | Description |
|---------|----|------|-------------|
| mkdir | 15 | path | Create directory |
| unlink | 16 | path | Delete file |
| rename | 17 | oldpath, newpath | Rename file |
| getcwd | 18 | buf, size | Get current working directory |
| chdir | 19 | path | Change current directory |
| stat | 20 | path, statbuf | Get file info (size, type, etc.) |

### stat struct (minimal)
```c
struct stat {
    uint32_t st_size;    // file size
    uint8_t  st_type;    // 0=file, 1=directory
    // extend as needed
};
```

## Utilities to Implement

### Essential (build first)
| App | Description | Syscalls needed |
|-----|-------------|-----------------|
| `ls` | List directory contents | readdir (already exists as FAT12 readdir) |
| `cat` | Print file to stdout | open, read, write |
| `pwd` | Print working directory | getcwd |
| `cd` | Change directory | chdir (needs kernel cwd tracking) |
| `mkdir` | Create directory | mkdir |
| `rm` | Remove file | unlink |
| `touch` | Create empty file | open (with O_CREAT) or create |
| `clear` | Clear terminal screen | write (escape sequence) |

### Useful (build second)
| App | Description |
|-----|-------------|
| `cp` | Copy file (open src, open dst, read/write loop) |
| `mv` | Move file (rename, or cp + rm) |
| `head` | Print first N lines |
| `tail` | Print last N lines |
| `wc` | Count lines, words, bytes |
| `echo` | Print arguments (already exists) |
| `ps` | List running processes (needs syscall) |
| `kill` | Send signal (already has syscall) |

### Nice-to-have (build third)
| App | Description |
|-----|-------------|
| `grep` | Basic string search in files |
| `hexdump` / `xxd` | Hex view |
| `ls -l` | Long format listing |
| `find` | Find files by name |
| `tree` | Directory tree view |
| `chmod` | Change permissions (low priority) |

## Kernel Changes Needed

### Process CWD tracking
- Add `char cwd[256]` to PCB
- `chdir()` updates it
- `getcwd()` reads it
- Default cwd = "/"

### readdir syscall
- Wraps VFS readdir_fs / FAT12 readdir
- Returns dirent structs to userspace

### stat syscall
- Look up path via VFS
- Return size and type flags

### open with O_CREAT (optional)
- Extend open syscall flags
- Create file if it doesn't exist

## Build Order
1. Add mkdir, chdir, getcwd, stat, unlink syscalls to kernel
2. Write `ls` (test readdir)
3. Write `cat` (test read)
4. Write `pwd` / `cd` (test getcwd/chdir)
5. Write `mkdir` / `rm` / `touch`
6. Update shell to support cd, clear as builtins
7. Write remaining utilities
