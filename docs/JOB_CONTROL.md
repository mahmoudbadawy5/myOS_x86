# Plan: Job Control & Signal Handling

## Current State

- OS boots, shell runs, apps dynamically linked via libc.so/libmath.so
- fork/wait/exec/pipes/dup2/redirection all working
- Signal infrastructure complete: per-process signal disposition, delivery via schedule(), sigreturn
- `signal()` syscall (#29) — userland signal handlers
- `sigprocmask()` syscall (#32) — block/unblock signals
- `setpgid()` syscall (#30) — process groups, foreground tracking
- `waitpid()` syscall (#31) — wait for specific child, WNOHANG
- Ctrl+C / Ctrl+Z in keyboard handler send signals to foreground process group
- Shell ignores SIGINT/SIGTSTP, children reset to SIG_DFL before exec
- Background jobs: `cmd &`, `jobs`/`fg`/`bg` builtins
- Signal masks: auto-block during handler, sigprocmask for user control

## Architecture

### Process Groups

Each pipeline creates a new process group. The first child's PID becomes the PGID. All children in the pipeline share this PGID. The shell tracks which group is in the foreground.

```
Shell (PID 1, PGID 1, session leader)
├── fg group: cat | grep foo  (PGID=2, fg)
│   ├── cat (PID 2, PGID 2)
│   └── grep (PID 3, PGID 2)
└── bg group: sleep 100 &    (PGID=4, bg)
    └── sleep (PID 4, PGID 4)
```

### Signal Disposition

Each process has a disposition table per signal:
- **SIG_DFL (0)**: Default action (varies per signal)
- **SIG_IGN (1)**: Ignore
- **User handler address**: Jump to handler on delivery

| Signal | Number | Default | Blockable |
|--------|--------|---------|-----------|
| SIGHUP | 1 | Kill | Yes |
| SIGINT | 2 | Kill | Yes |
| SIGQUIT | 3 | Kill | Yes |
| SIGKILL | 9 | Kill | **No** |
| SIGTERM | 15 | Kill | Yes |
| SIGSTOP | 17 | Stop | **No** |
| SIGCONT | 18 | Continue | Yes |
| SIGTSTP | 20 | Stop | Yes |
| SIGCHLD | 19 | Ignore | Yes |
| SIGPIPE | 13 | Kill | Yes |
| SIGALRM | 14 | Kill | Yes |

### Signal Delivery Flow

1. Signal sent (kill syscall, keyboard, child exit)
2. `signal_pending` set on target PCB
3. On next `schedule()` → `switch_to_process()` (return to userspace):
   - Check `signal_pending != 0`
   - Look up disposition: ignore → clear and continue; kill → terminate; stop → set STOPPED; catch → set up signal frame on user stack, redirect EIP to handler
4. When handler returns (via `sigreturn` syscall), restore original register state

### Keyboard → Foreground Group

Ctrl+C / Ctrl+Z / Ctrl+\ no longer target `current_process` directly. Instead:
1. Kernel tracks `foreground_pgid` (the PGID of the foreground process group)
2. Keyboard handler sends signal to all processes with `pgid == foreground_pgid`
3. Background processes that try to read stdin get SIGTTIN (stopped)

---

## Phase 1: Signal Constants & Disposition Table

**Goal**: Define signal types and per-process signal disposition.

### Files to modify

| File | Changes |
|------|---------|
| `kernel/src/include/proc/process.h` | Add `pgid`, `signal_disposition[NSIG]`, `sigmask`, `stopped_by` to PCB; define signal constants |
| `kernel/src/proc/process.c` | Initialize new fields in `create_process`, `fork_process` |
| `kernel/src/include/fs/pipe.h` | No changes needed |

### PCB additions

```c
#define NSIG 32
#define SIG_DFL  0
#define SIG_IGN  1

/* Signal numbers */
#define SIGHUP    1
#define SIGINT    2
#define SIGQUIT   3
#define SIGILL    4
#define SIGKILL   9
#define SIGUSR1  10
#define SIGSEGV  11
#define SIGUSR2  12
#define SIGPIPE  13
#define SIGALRM  14
#define SIGTERM  15
#define SIGSTOP  17
#define SIGCONT  18
#define SIGCHLD  19
#define SIGTSTP  20
#define SIGTTIN  21
#define SIGTTOU  22

typedef struct pcb {
    /* ... existing fields ... */
    uint32_t pgid;                    /* Process group ID */
    uint32_t signal_disposition[NSIG]; /* SIG_DFL, SIG_IGN, or handler addr */
    uint32_t sigmask;                 /* Blocked signals bitmask */
    uint32_t signal_pending;          /* Pending signals bitmask (replaces old int) */
    uint32_t stopped_by;              /* Signal that stopped this process (0 = not stopped) */
} pcb_t;
```

### Default dispositions

Set in `create_process` and `fork_process`:
```c
for (int i = 0; i < NSIG; i++)
    proc->signal_disposition[i] = SIG_DFL;
proc->signal_disposition[SIGCHLD] = SIG_IGN; /* Default: ignore SIGCHLD */
proc->pgid = proc->pid; /* Own process group by default */
proc->sigmask = 0;
proc->signal_pending = 0;
proc->stopped_by = 0;
```

### Validation

Build with `make clean && make`. No runtime changes yet — just data structure additions.

---

## Phase 2: Signal Delivery in Scheduler

**Goal**: Replace the "any signal kills" logic with proper signal dispatch.

### Files to modify

| File | Changes |
|------|---------|
| `kernel/src/proc/process.c` | Rewrite signal check in `schedule()` |
| `kernel/src/arch/syscalls.c` | Add `sys_sigreturn` syscall |

### Schedule signal check (replaces lines 259-266)

```c
if (current_process && current_process->signal_pending) {
    uint32_t pending = current_process->signal_pending;
    for (int sig = 1; sig < NSIG; sig++) {
        if (!(pending & (1 << sig))) continue;

        /* SIGSTOP and SIGCONT have special semantics */
        if (sig == SIGSTOP) {
            current_process->signal_pending &= ~(1 << sig);
            current_process->stopped_by = SIGSTOP;
            current_process->state = PROCESS_STATE_BLOCKED;
            break;
        }
        if (sig == SIGCONT) {
            current_process->signal_pending &= ~(1 << sig);
            current_process->stopped_by = 0;
            current_process->state = PROCESS_STATE_READY;
            continue; /* Check for more pending signals */
        }

        uint32_t disp = current_process->signal_disposition[sig];
        if (disp == SIG_IGN) {
            current_process->signal_pending &= ~(1 << sig);
            continue;
        }
        if (disp == SIG_DFL) {
            /* Default: kill */
            current_process->signal_pending &= ~(1 << sig);
            current_process->state = PROCESS_STATE_TERMINATED;
            unblock_parent(current_process->pid);
            current_process = NULL;
            break;
        }
        /* User handler: set up signal frame (Phase 3) */
        /* For now, just kill */
        current_process->signal_pending &= ~(1 << sig);
        current_process->state = PROCESS_STATE_TERMINATED;
        unblock_parent(current_process->pid);
        current_process = NULL;
        break;
    }
}
```

### Add `sigreturn` syscall (#28)

When a signal handler returns, the libc `sigreturn()` function triggers this syscall to restore the original register state.

```c
int32_t syscall_sigreturn(struct regs *regs) {
    /* The signal frame was set up on the user stack.
     * Restore the original trap frame from the saved location.
     * For Phase 2 (no user handlers yet), this is a no-op placeholder. */
    return 0;
}
```

Update: `MAX_SYSCALLS = 33`, `MAX_SYSCALL EQU 32` in ASM, add to syscall table and libc wrappers.

### Validation

- `kill <pid> 9` should still kill (SIGKILL)
- `kill <pid> 17` should stop a process (SIGSTOP)
- `kill <pid> 18` should resume a stopped process (SIGCONT)
- Existing Ctrl+C behavior preserved (keyboard handler sends SIGINT)

---

## Phase 3: Userland Signal Delivery

**Goal**: Allow processes to register signal handlers that run in userspace.

### Files to modify

| File | Changes |
|------|---------|
| `kernel/src/arch/syscalls.c` | Add `sys_signal` and enhance `sys_sigreturn` |
| `kernel/src/proc/process.c` | Set up signal frame on user stack in `schedule()` |
| `libc/src/syscalls.c` | Add `signal()`, `sigreturn()` wrappers |
| `libc/src/include/syscalls.h` | Declare new libc functions |
| `libc/src/include/test.h` | Add `signal()` typedef and wrapper |

### New syscalls

**`signal(signum, handler)` — syscall #29**
```c
int32_t syscall_signal(struct regs *regs) {
    uint32_t signum = regs->ebx;
    uint32_t handler = regs->ecx;
    if (signum >= NSIG || signum == 0 || signum == SIGKILL || signum == SIGSTOP)
        return -1;
    if (handler != SIG_DFL && handler != SIG_IGN && !is_user_ptr((void *)handler))
        return -1;
    current_process->sig.disposition[signum] = handler;
    return 0;
}
```

**`sigreturn()` — syscall #28** (enhanced)
When a signal handler is invoked, the kernel pushes a "signal frame" onto the user stack:
```
[original EIP]  [original EAX..EDI]  [signal number]
```
The handler is called with `signum` as argument. When the handler calls `sigreturn()`, the kernel:
1. Pops the signal frame from the user stack
2. Restores the original registers
3. Returns to the original EIP

### Signal frame setup (in schedule(), when catching a signal)

```c
/* Save current state as a signal frame on user stack */
uint32_t user_esp = current_process->regs.useresp;
user_esp -= sizeof(struct regs);
struct regs *sframe = (struct regs *)user_esp;
/* Copy current registers into signal frame */
*sframe = /* current state */;
sframe->eax = sig; /* signal number as argument */

/* Redirect EIP to handler */
current_process->regs.useresp = user_esp;
/* Push sigreturn address and jump to handler */
uint32_t *stack = (uint32_t *)user_esp;
stack--; *stack = /* sigreturn trampoline address */;
current_process->regs.eip = disp; /* handler address */
```

### libc wrapper

```c
typedef void (*sighandler_t)(int);

sighandler_t signal(int signum, sighandler_t handler) {
    return (sighandler_t)sys_signal(signum, (uint32_t)handler);
}
```

### Validation

Write a test app that:
1. Registers a handler for SIGINT via `signal(SIGINT, handler)`
2. Handler prints "Caught SIGINT" and returns
3. Parent sends `kill(child_pid, SIGINT)`
4. Child's handler runs, then child continues

---

## Phase 4: Process Groups & Foreground Tracking

**Goal**: Add process group management and terminal foreground group tracking.

### Files to modify

| File | Changes |
|------|---------|
| `kernel/src/include/proc/process.h` | Add `foreground_pgid` global |
| `kernel/src/proc/process.c` | Initialize pgid, add `setpgid` logic |
| `kernel/src/arch/syscalls.c` | Add `sys_setpgid`, `sys_getpgrp` syscalls |
| `kernel/src/drivers/kbd.c` | Send signals to foreground group, not just current_process |
| `kernel/src/include/arch/syscalls.h` | Update MAX_SYSCALLS |
| `kernel/src/arch/syscalls_asm.asm` | Update MAX_SYSCALL |
| `libc/src/syscalls.c` | Add libc wrappers |
| `libc/src/include/syscalls.h` | Declare wrappers |
| `libc/src/include/test.h` | Add wrapper declarations |

### New syscalls

**`setpgid(pid, pgid)` — syscall #30**
```c
int32_t syscall_setpgid(struct regs *regs) {
    uint32_t pid = regs->ebx;
    uint32_t pgid = regs->ecx;
    if (pid == 0 && pgid == 0) { foreground_pgid = 0; return 0; }
    if (pid == 0) { foreground_pgid = pgid; return 0; }
    pcb_t *proc = get_process_by_pid(pid);
    if (!proc) return -1;
    if (pgid == 0) pgid = pid;
    proc->pgid = pgid;
    foreground_pgid = pgid;
    return 0;
}
```

### Global foreground tracking

```c
/* process.h */
extern uint32_t foreground_pgid; /* PGID of foreground process group */

/* process.c */
uint32_t foreground_pgid = 0; /* 0 = shell is foreground (no child running) */
```

### Keyboard handler update (kbd.c)

Replace the current Ctrl+C handler:
```c
/* OLD: current_process->signal_pending = 2; */
/* NEW: send SIGINT to all processes in foreground_pgid */
if (foreground_pgid != 0) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        pcb_t *p = &process_table[i];
        if (p->state != PROCESS_STATE_TERMINATED && p->pgid == foreground_pgid) {
            p->signal_pending |= (1 << SIGINT);
        }
    }
}
```

Same for Ctrl+Z (SIGTSTP) and Ctrl+\ (SIGQUIT).

### Validation

- `cat` running in foreground → Ctrl+C kills it
- `cat &` running in background → Ctrl+C does NOT kill it
- `sleep 100 &` → Ctrl+Z does NOT stop it (it's background)
- `sleep 100` in foreground → Ctrl+Z stops it

---

## Phase 5: Shell Job Control

**Goal**: Shell tracks jobs, supports `&`, `jobs`, `fg`, `bg`.

### Files to modify

| File | Changes |
|------|---------|
| `apps/shell.c` | Parse `&`, maintain job list, add `jobs`/`fg`/`bg` builtins, async child reaping |

### Job data structure

```c
#define MAX_JOBS 16

typedef struct {
    int job_id;         /* Display ID (1, 2, 3, ...) */
    int pgid;           /* Process group ID */
    char cmd[64];       /* Command string */
    int running;        /* 1 = running, 0 = stopped */
    int foreground;     /* 1 = foreground, 0 = background */
} job_t;

static job_t jobs[MAX_JOBS];
static int num_jobs = 0;
static int next_job_id = 1;
```

### Parse `&` suffix

In `parse_line()`: if the last token of a stage is `&`, mark it as a background job. Set a `background` flag on the stage.

```c
/* At end of arg parsing, before args[argc] = 0: */
if (st->argc > 0 && strcmp(st->args[st->argc - 1], "&") == 0) {
    st->args[--st->argc] = 0; /* Remove & from args */
    st->background = 1;
}
```

### Fork with process groups

When forking a pipeline:
1. Fork first child → its PID becomes the PGID
2. Call `setpgid(child_pid, child_pid)` on the first child
3. Fork remaining children → `setpgid(child_pid, first_child_pid)`
4. If foreground: `foreground_pgid = pgid`
5. Add job to job list
6. If foreground: `wait()` for the group. If background: don't wait.

### Async child reaping (SIGCHLD)

Instead of blocking `wait()`, the shell should:
1. Set `SIGCHLD` disposition to a handler
2. In the handler: call non-blocking `waitpid(-1, &status, WNOHANG)` in a loop
3. Update job status (remove completed jobs, mark stopped jobs)
4. Print `[N] Done` or `[N] Stopped` messages

**Simplified approach** (without full SIGCHLD handler):
- Before each prompt, do non-blocking wait: `waitpid(-1, &status, WNOHANG)`
- This reaps any finished children without blocking

### `jobs` builtin

```c
void list_jobs(void) {
    for (int i = 0; i < num_jobs; i++) {
        job_t *j = &jobs[i];
        print("[");
        print_int(j->job_id);
        print("] ");
        print(j->running ? "Running" : "Stopped");
        print("  ");
        print(j->cmd);
        print("\n");
    }
}
```

### `fg` builtin

```c
void fg_job(int job_id) {
    job_t *j = find_job(job_id);
    if (!j) { print("fg: no such job\n"); return; }
    foreground_pgid = j->pgid;
    /* Send SIGCONT if stopped */
    if (!j->running) {
        for (int i = 0; i < MAX_PROCESSES; i++) {
            if (process_table[i].pgid == j->pgid)
                process_table[i].signal_pending |= (1 << SIGCONT);
        }
        j->running = 1;
    }
    /* Wait for the job to finish or be stopped */
    wait_for_job(j);
    foreground_pgid = 0; /* Shell is foreground again */
}
```

### `bg` builtin

```c
void bg_job(int job_id) {
    job_t *j = find_job(job_id);
    if (!j) { print("bg: no such job\n"); return; }
    /* Send SIGCONT */
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].pgid == j->pgid)
            process_table[i].signal_pending |= (1 << SIGCONT);
    }
    j->running = 1;
    print("[");
    print_int(j->job_id);
    print("] Continued\n");
}
```

### Background process stdin protection

When a background process tries to read from stdin (fd 0 = keyboard):
- The read should return -1 or the process should receive SIGTTIN
- Simplest approach: background processes that try to `read()` on fd 0 get stopped (SIGTTIN)

This can be implemented in `syscall_read`: if `fd == 0` and `current_process->pgid != foreground_pgid`, send SIGTSTP and return -1.

### Validation

Test scenarios:
1. `sleep 100 &` → shell continues immediately, `jobs` shows it running
2. `sleep 100 &` then `fg 1` → brings sleep to foreground, Ctrl+C kills it
3. `cat &` → tries to read stdin, gets stopped (SIGTTIN)
4. `sleep 5 & sleep 10 &` → two background jobs, `jobs` shows both
5. `sleep 100` then Ctrl+Z → stops, `jobs` shows Stopped, `bg 1` resumes, `fg 1` brings back

---

## Phase 6: SIGCHLD & Non-blocking Wait

**Goal**: Shell doesn't block waiting for background jobs.

### Files to modify

| File | Changes |
|------|---------|
| `kernel/src/arch/syscalls.c` | Add `waitpid(pid, status, options)` syscall |
| `kernel/src/proc/process.c` | `find_terminated_child` with PID filter |
| `libc/src/syscalls.c` | Add `waitpid()` wrapper |
| `libc/src/include/test.h` | Add `waitpid()` declaration |

### New syscall: `waitpid(pid, options)` — syscall #31

```c
int32_t syscall_waitpid(struct regs *regs) {
    uint32_t req_pid = regs->ebx;     /* 0 = any child, >0 = specific PID */
    uint32_t options = regs->ecx;     /* WNOHANG=1 */
    uint32_t my_pid = current_process->pid;

    /* Check for terminated child matching pid */
    uint32_t dead = find_terminated_child(my_pid);
    if (dead != 0 && (req_pid == 0 || req_pid == dead)) {
        remove_child_from_parent(current_process, dead);
        pcb_t *child = get_process_by_pid(dead);
        if (child) process_cleanup_child(child);
        return dead;
    }

    /* WNOHANG: return 0 immediately */
    if (options & 1) return 0;

    /* Block until the requested child exits */
    current_process->waiting_on_pid = req_pid;
    while (has_live_children(my_pid)) {
        /* re-check terminated/stopped */
        ...
        current_process->state = PROCESS_STATE_BLOCKED;
        schedule(regs);
    }
    return -1;
}
```

### New syscall: `sigprocmask(how, set, oldset)` — syscall #32

```c
int32_t syscall_sigprocmask(struct regs *regs) {
    uint32_t how = regs->ebx;
    sigset_t *set = (sigset_t *)regs->ecx;
    sigset_t *oldset = (sigset_t *)regs->edx;
    if (oldset) *oldset = current_process->sig.mask;
    if (set) {
        uint32_t new_set = *set & ~(SIG_BIT(SIGKILL) | SIG_BIT(SIGSTOP));
        switch (how) {
            case SIG_BLOCK:   current_process->sig.mask |= new_set; break;
            case SIG_UNBLOCK: current_process->sig.mask &= ~new_set; break;
            case SIG_SETMASK: current_process->sig.mask = new_set; break;
        }
    }
    return 0;
}
```

### libc wrapper

```c
int waitpid(int pid, int *status, int options) {
    return sys_waitpid(pid, status, options);
}
```

### Shell integration

Before each prompt:
```c
/* Reap any finished background children (non-blocking) */
int pid;
while ((pid = waitpid(0, WNOHANG)) > 0) {
    int idx = find_job_by_pid(pid);
    if (idx >= 0)
        jobs[idx].status = JOB_DONE;
}
compact_jobs();
```

### Validation

- `sleep 1 &` → shell prompts immediately
- Wait 2 seconds → shell prints `[Done] PID <n>` before next prompt
- `sleep 1 & sleep 2 &` → both get reaped

---

## Implementation Order

| Step | What | Depends on | Syscalls added |
|------|------|------------|----------------|
| 1 | Signal constants & PCB fields | — | — |
| 2 | Signal delivery in scheduler | 1 | sigreturn (#28) |
| 3 | Userland signal delivery | 2 | signal (#29) |
| 4 | Process groups & foreground | 1 | setpgid (#30) |
| 5 | Shell job control | 2, 3, 4 | — |
| 6 | SIGCHLD & non-blocking wait | 5 | waitpid (#31) |
| 7 | Signal masks | 2 | sigprocmask (#32) |

**Total new syscalls**: 5 (sigreturn, signal, setpgid, waitpid, sigprocmask)
**New process states**: PROCESS_STATE_STOPPED (or reuse BLOCKED with `stopped_by`)
**New PCB fields**: pgid, signal_disposition[], sigmask, stopped_by, sig (signal_state_t)
**New shell builtins**: jobs, fg, bg

## Testing Checklist

- [ ] `kill <pid> 9` kills a process (SIGKILL)
- [ ] `kill <pid> 17` stops a process (SIGSTOP)
- [ ] `kill <pid> 18` resumes a stopped process (SIGCONT)
- [ ] Ctrl+C kills foreground process
- [ ] Ctrl+Z stops foreground process
- [ ] `sleep 100 &` runs in background, shell continues
- [ ] `jobs` lists background/stopped jobs
- [ ] `fg %1` brings job to foreground
- [ ] `bg %1` resumes stopped job in background
- [ ] `cat &` gets SIGTTIN (stopped trying to read stdin)
- [ ] Background process exits → shell prints `[Done]` message
- [ ] Pipeline `cat | grep foo &` runs as single background group
- [ ] Signal handler: register handler, send signal, handler runs, process continues
- [ ] Ctrl+C in foreground pipeline kills ALL processes in the group
