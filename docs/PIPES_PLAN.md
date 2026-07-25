# Plan: Pipes, Redirection, Shell Pipes

## Current State

- OS boots, shell runs, apps dynamically linked via libc.so/libmath.so
- fork/wait working (CR3 fix applied)
- pipe() syscall exists but non-blocking (reads return 0 when empty, writes drop data when full)
- No dup2, no shell pipe (|) or redirect (>, <, >>) support
- dup() stores same FILE* without refcount (dangling pointer risk)
- fork() doesn't increment pipe_buf_t.refcount (premature deallocation risk)
- All recent inline bugs fixed (path bounds, mmap, munmap, elf leaks, process rollback, etc.)

## Phase 1: Kernel Fixes

### 1.1 Fix dup() refcounts
**File**: `kernel/src/arch/syscalls.c` lines 482-499

Current: stores same `FILE*` in two slots, no refcount changes.
Fix:
- Allocate new `FILE` via malloc
- Copy `flags` and `file` pointer from original
- Increment `fs_node_t.refcount`
- If pipe (`node->flags == FS_PIPE`), increment `pipe_buf_t.refcount`
- Store new FILE in first free slot

### 1.2 Add dup2(oldfd, newfd) syscall
**New syscall #27** (table has 27 entries 0-26, next slot is 27)

Files to update:
- `kernel/src/arch/syscalls.c` — add `syscall_dup2(struct regs*)`
- `kernel/src/include/arch/syscalls.h` — `MAX_SYSCALLS = 28`, add declaration
- `kernel/src/arch/syscalls_asm.asm` — `MAX_SYSCALL EQU 27`
- `libc/src/syscalls.c` — add `sys_dup2(int oldfd, int newfd)`
- `libc/src/include/syscalls.h` — add `sys_dup2()` declaration
- `libc/src/include/test.h` — add `int dup2(int oldfd, int newfd);` wrapper

Logic:
1. Validate old_fd and new_fd < MAX_FILES
2. Check old_fd is open
3. If new_fd is open, close it (call syscall_close logic)
4. Allocate new FILE, copy flags/file from old
5. Increment refcounts (same as dup fix)
6. Store in `files_open[new_fd]`
7. Return new_fd

### 1.3 Fix fork() pipe_buf refcount
**File**: `kernel/src/proc/process.c` lines 515-528 (fd copy loop)

Current: increments `fs_node_t.refcount` but not `pipe_buf_t.refcount`.
Fix: when `new_fp->file->flags == FS_PIPE`, also increment `((pipe_buf_t*)new_fp->file->ptr)->refcount`.

## Phase 2: Pipe Blocking

### 2.1 Add readers/writers to pipe_buf_t
**File**: `kernel/src/include/fs/pipe.h`

```c
typedef struct {
    uint8_t buf[PIPE_BUF_SIZE];
    uint32_t read_pos;
    uint32_t write_pos;
    uint32_t count;
    uint32_t refcount;
    uint32_t readers;  // number of open read endpoints
    uint32_t writers;  // number of open write endpoints
} pipe_buf_t;
```

Use `node->impl` field (currently unused for pipes) to store role: 1=reader, 0=writer.

### 2.2 Update pipe_create
**File**: `kernel/src/fs/pipe.c`

- Set `pb->readers = 1; pb->writers = 1;`
- Set `read_node->impl = 1; write_node->impl = 0;`

### 2.3 Update pipe_close_fn
**File**: `kernel/src/fs/pipe.c`

- Check `node->impl` to decrement `readers` or `writers`

### 2.4 Update pipe_read_fn / pipe_write_fn — busy-wait with yield
**File**: `kernel/src/fs/pipe.c`

**pipe_read_fn**:
```c
while (read < total) {
    if (pb->count == 0) {
        if (pb->writers == 0)
            return read;  // EOF — no writers left
        __asm__ __volatile__("sti; hlt");  // yield until timer interrupt
        continue;
    }
    buffer[read++] = pb->buf[pb->read_pos];
    pb->read_pos = (pb->read_pos + 1) % PIPE_BUF_SIZE;
    pb->count--;
}
```

**pipe_write_fn**:
```c
while (written < total) {
    if (pb->count >= PIPE_BUF_SIZE) {
        if (pb->readers == 0)
            return 0;  // broken pipe — no readers
        __asm__ __volatile__("sti; hlt");  // yield until timer interrupt
        continue;
    }
    pb->buf[pb->write_pos] = buffer[written++];
    pb->write_pos = (pb->write_pos + 1) % PIPE_BUF_SIZE;
    pb->count++;
}
```

## Phase 3: Shell Pipes & Redirection

### 3.1 Refactor shell parser
**File**: `apps/shell.c`

New `parse_line()` that scans for `|`, `>`, `>>`, `<` tokens:
- Split line into pipe stages (split on `|`)
- For each stage, identify redirections (split on `>`, `>>`, `<`)
- Return structured result: array of stages, each with args[], in_file, out_file, append flag

### 3.2 Add fork+exec+redirection
**File**: `apps/shell.c`

All external commands use fork+exec pattern:
```c
pid_t pid = fork();
if (pid == 0) {
    // Child: apply redirections
    if (in_file) { fd = open(in_file, "r"); dup2(fd, 0); close(fd); }
    if (out_file) { fd = open(out_file, append ? "a" : "w"); dup2(fd, 1); close(fd); }
    // Reconstruct cmdline and exec
    exit(exec(cmdline));
}
// Parent: wait
wait();
```

Builtins (no fork/exec): `help`, `clear`, `cd`, `pwd`

### 3.3 Add pipe execution
**File**: `apps/shell.c`

For `cmd1 | cmd2`:
```c
int fds[2];
pipe(fds);

// Fork left child (stdout -> pipe write end)
pid_t left = fork();
if (left == 0) {
    close(fds[0]);
    dup2(fds[1], 1);
    close(fds[1]);
    exec(left_cmd);
    exit(1);
}

// Fork right child (stdin <- pipe read end)
pid_t right = fork();
if (right == 0) {
    close(fds[1]);
    dup2(fds[0], 0);
    close(fds[0]);
    exec(right_cmd);
    exit(1);
}

// Parent: close pipe ends, wait both
close(fds[0]);
close(fds[1]);
wait();  // wait for left
wait();  // wait for right
```

## Phase 4: Test

### 4.1 Create apps/test_pipe.c
Tests:
- Fork+pipe: parent writes "hello", child reads and prints
- EOF detection: child reads after parent closes write end
- Multiple writes/reads through pipe

## Implementation Order

| Step | What | Depends on |
|------|------|------------|
| 1.1 | Fix dup() refcounts | — |
| 1.2 | Add dup2() syscall | — |
| 1.3 | Fix fork() pipe refcount | — |
| 2.1 | Add readers/writers to pipe_buf_t | — |
| 2.2 | Update pipe_create | 2.1 |
| 2.3 | Update pipe_close_fn | 2.1 |
| 2.4 | Block reads/writes | 2.1, 2.2, 2.3 |
| 3.1 | Refactor shell parser | 1.2 |
| 3.2 | Shell fork+exec+redirect | 1.1, 1.2, 2.4 |
| 3.3 | Shell pipe execution | 2.4, 3.1, 3.2 |
| 4.1 | test_pipe.c | 2.4 |

## Files Modified

| File | Changes |
|------|---------|
| `kernel/src/arch/syscalls.c` | Fix dup, add dup2 |
| `kernel/src/include/arch/syscalls.h` | MAX_SYSCALLS=28, dup2 decl |
| `kernel/src/arch/syscalls_asm.asm` | MAX_SYSCALL EQU 27 |
| `kernel/src/proc/process.c` | fork pipe_buf refcount |
| `kernel/src/fs/pipe.c` | blocking read/write, readers/writers |
| `kernel/src/include/fs/pipe.h` | readers/writers fields |
| `libc/src/syscalls.c` | sys_dup2() |
| `libc/src/include/syscalls.h` | sys_dup2() decl |
| `libc/src/include/test.h` | dup2() wrapper |
| `apps/shell.c` | Rewrite: pipes, redirects, fork+exec |
| `apps/test_pipe.c` | New test file |

## After Pipes: Signal Handling

Next feature (per earlier decision):
- `kill(pid, sig)` — exists (syscall #13), needs SIGKILL/SIGTERM semantics
- `signal(sig, handler)` — new syscall for userland handlers
- `SIGPIPE` — sent to writer when reader closes pipe end
- Process groups for job control
