# Ring 3 Process Infrastructure — Current State Audit

This document captures the exact state of all ring-3 (user mode) infrastructure as of
Phase 3 completion. It is intended as the starting reference when implementing true
ring-3 execution hardening, fork(), user-mode page fault handling, or any process
management work.

---

## 1. GDT Layout

File: `kernel/src/arch/gdt.c`

| Index | Selector | Segment        | Access | Granularity | Description |
|-------|----------|----------------|--------|-------------|-------------|
| 0     | 0x00     | NULL           | 0x00   | 0x00        | Unused      |
| 1     | 0x08     | Kernel Code    | 0x9A   | 0xCF        | Ring 0, 32-bit, base=0, limit=4GB |
| 2     | 0x10     | Kernel Data    | 0x92   | 0xCF        | Ring 0, 32-bit, base=0, limit=4GB |
| 3     | 0x1B     | User Code      | 0xFA   | 0xCF        | Ring 3, 32-bit, base=0, limit=4GB |
| 4     | 0x23     | User Data      | 0xF2   | 0xCF        | Ring 3, 32-bit, base=0, limit=4GB |
| 5     | 0x28     | TSS            | 0x89   | 0x00        | Task State Segment |

**Status:** Ring 3 segments are correctly defined. Code segment selector is 0x1B (RPL=3),
data segment selector is 0x23 (RPL=3).

---

## 2. TSS (Task State Segment)

File: `kernel/src/arch/tss.c`

- `ss0 = 0x10` (kernel data segment) — correct
- `esp0` is set to `exception_stack + TSS_EXCEPTION_STACK_SIZE` at init
- `tss_set_stack(kernel_ss, kernel_esp)` updates `ss0` and `esp0` dynamically
- Called in `schedule()` as: `tss_set_stack(0x10, next->kernel_stack_top)`
- `iomap_base = sizeof(tss_entry_t)` (no I/O permission bitmap)

**Status:** TSS correctly configured. `esp0` is updated per-process in `schedule()` so that
on interrupts/syscalls the CPU switches to the correct kernel stack.

---

## 3. Context Switch (`switch_to_process`)

File: `kernel/src/proc/process_asm.asm`

### PCB field offsets (hardcoded):
```
PCB_OFFSET_PROCESS_STATE  = 4   (state enum)
PCB_OFFSET_KERNEL_STACK   = 8   (kernel_stack_top — IRET frame address)
PCB_OFFSET_REGS_ESP       = 40  (regs.esp — saved trap frame pointer)
PCB_OFFSET_CR3            = 52  (regs.cr3 — page directory physical address)
```

### How it works:

**First run (PROCESS_STATE_NEW):**
1. Loads `esp` from `PCB_OFFSET_KERNEL_STACK` (the IRET frame address)
2. Sets all data segment registers to 0x23 (user data)
3. Executes `iretd` which pops EIP, CS(0x1B), EFLAGS(IF=1), ESP, SS(0x23) — transitions to ring 3

**Subsequent runs:**
1. Loads `esp` from `PCB_OFFSET_REGS_ESP` (saved trap frame pointer)
2. Pops GS, FS, ES, DS, general regs, skips error code + interrupt number
3. Executes `iretd` which pops EIP, CS, EFLAGS, ESP, SS

### Key finding: Both paths use `iretd`

The "subsequent runs" path also uses `iretd` (line 89), NOT `jmp`. This means the
scheduler correctly returns to ring 3 on context switches. The CS/SS values in the trap
frame are set by the CPU when the interrupt/syscall happened (pushing user CS/SS), and
restored by `iretd`.

**Status:** Context switch correctly uses `iretd` on both first-run and subsequent runs.
Ring 3 transition works on both paths.

---

## 4. Process Creation

File: `kernel/src/proc/process.c`

### `load_program()` — sets up ring 3 register state:
```c
proc->regs.eflags = 0x202;    // IF=1 (interrupts enabled)
proc->regs.cs = 0x1B;         // User code segment (ring 3)
proc->regs.ds = 0x23;         // User data segment (ring 3)
proc->regs.es = 0x23;
proc->regs.fs = 0x23;
proc->regs.gs = 0x23;
proc->regs.ss = 0x23;         // User stack segment (ring 3)
```

### `create_process()` — allocates resources:
- `kernel_stack_alloc` = malloc'd 8192 bytes (base)
- `kernel_stack_bottom` = `kernel_stack_alloc` (lowest mapped page)
- `kernel_stack_top` = `kstack_base - 32`, aligned to 16 bytes (IRET frame address)
- Page directory cloned from kernel via `vmm_clone_directory()`
- `files_open[0]` = stdin (FILE struct allocated, points to `stdin_node`)
- `files_open[1]` = stdout (FILE struct allocated, points to `stdout_node`)
- CWD inherited from parent (or "/" for init)
- State set to `PROCESS_STATE_NEW`

### IRET frame layout at `kernel_stack_top`:
```
frame[0] = user EIP (from ELF entry point)
frame[1] = 0x1B (user CS)
frame[2] = 0x202 (EFLAGS, IF=1)
frame[3] = user ESP (from argv setup in load_program)
frame[4] = 0x23 (user SS)
```

**Status:** Process creation correctly sets up ring 3 segments. The IRET frame is
properly built on the kernel stack.

---

## 5. Syscall Gate

File: `kernel/src/arch/syscalls.c`, `kernel/src/arch/syscalls_asm.asm`

### IDT entry:
```c
idt_set_gate(0x80, (unsigned)handle_syscalls, 0x08, 0xEE);
```
- DPL=3 (0xEE = present, ring 3, interrupt gate) — allows user-mode `int $0x80`

### ASM handler (`handle_syscalls`):
1. CPU pushes user SS, ESP, EFLAGS, CS, EIP (from ring 3)
2. Handler pushes dummy err_code(0) and int_number(0x80)
3. Pushes DS, ES, FS, GS, general regs (pusha)
4. Switches to kernel segments (0x10)
5. Calls C handler via `syscalls[eax]` table
6. Writes return value into saved EAX slot
7. Restores segments, general regs, pops err_code/int_num
8. `iretd` — returns to ring 3

### Register convention (syscall args):
```
eax = syscall number
ebx = arg1
ecx = arg2
edx = arg3
esi = arg4
```

**Status:** Syscall gate correctly configured for ring 3 access. Handler properly saves
and restores full register state. Return to ring 3 via `iretd`.

---

## 6. Trap Frame / `struct regs`

File: `kernel/src/include/isr.h` (inferred from ASM offsets)

```c
// Stack layout after push gs/fs/es/ds + pusha:
[esp+0]  = gs
[esp+4]  = fs
[esp+8]  = es
[esp+12] = ds
[esp+16] = edi
[esp+20] = esi
[esp+24] = ebp
[esp+28] = (ignored esp)
[esp+32] = ebx
[esp+36] = edx
[esp+40] = ecx
[esp+44] = eax
[esp+48] = error_code (dummy 0)
[esp+52] = interrupt_number
[esp+56] = eip (pushed by CPU on interrupt)
[esp+60] = cs  (pushed by CPU)
[esp+64] = eflags (pushed by CPU)
[esp+68] = esp (pushed by CPU, user mode)
[esp+72] = ss  (pushed by CPU, user mode)
```

The `struct regs` maps directly to this layout. `regs->cs` at offset 60 holds the
user CS (0x1B) when called from ring 3.

---

## 7. Page Fault Handler & Kernel Stack Growth

File: `kernel/tools/isr_c_gen.py` (generates ISR handlers)

The page fault handler (ISR #14) checks if the fault address is within the current
process's kernel stack (one page below `kernel_stack_bottom`). If so, it maps a new
page on demand.

### Validation chain:
1. `current_process != NULL`
2. `current_process->kernel_stack_bottom > current_process->kernel_stack_alloc`
3. `fault_addr >= current_process->kernel_stack_bottom - BLOCK_SIZE`
4. `fault_addr < current_process->kernel_stack_bottom`
5. Maps new page at `current_process->kernel_stack_bottom`
6. `current_process->kernel_stack_bottom -= BLOCK_SIZE`

**Status:** Kernel stack grows on demand via page faults. Only validates current_process
(not scanning all processes). Validated against `kernel_stack_alloc` base.

---

## 8. User Pointer Validation

File: `kernel/src/arch/syscalls.c`

```c
static inline int is_user_ptr(const void *p)
{
    return (uint32_t)p < KERNEL_VIRTUAL_BASE && p != 0;
}
```

Used in: `syscall_readdir`, `syscall_stat`, `syscall_getcwd`, `syscall_chdir`,
`syscall_mkdir`, `syscall_unlink`, `syscall_ps`, `syscall_pipe`.

**Status:** Fast-path sanity check (rejects NULL and kernel-space pointers). The thorough
page-table validation is done by `is_page_mapped()` / `copy_to_user()` / `copy_from_user()`
which are called after the `is_user_ptr` check passes. Both layers are needed:
`is_user_ptr` avoids expensive page table walks for obviously bad pointers.

---

## 9. What Currently Works in Ring 3

- All processes run at CPL=3 (ring 3) on first execution and on context switch
- Syscall gate allows ring 3 `int $0x80` with DPL=3
- User code/data segments (0x1B/0x23) are correctly configured
- TSS switches to per-process kernel stack on ring transitions
- User pages are mapped in process page directories
- Processes can call all syscalls (24 total as of Phase 3)

---

## 10. What Does NOT Work / Is Missing

### 10.1 User-mode page faults kill the process (FIXED)
- Page fault handler checks CPL: if fault is from ring 3, kills the process and schedules
- Kernel faults still panic (correct behavior)
- Uses `kill_children_of()`, `unblock_parent()`, `schedule()` for cleanup

### 10.2 No `fork()` syscall
- `vmm_clone_directory()` exists but only clones kernel page table mappings
- Does NOT copy user pages (no copy-on-write)
- No mechanism to duplicate a process's address space

### 10.3 No process priority / nice
- Round-robin scheduler with fixed time slices
- No priority levels, no preemption control

### 10.4 No signal infrastructure
- `signal_pending` field exists on PCB but only checked for SIGINT (kill)
- No signal handler registration, no signal masks, no SIGTERM/SIGSEGV/SIGKILL

### 10.5 `copy_to_user` / `copy_from_user` / `is_page_mapped` (FIXED)
- `is_page_mapped()` walks the page directory + page table to check if a virtual address is present
- `copy_to_user()` / `copy_from_user()` validate every page in the range before copying
- `resolve_path()` uses `copy_from_user()` byte-by-byte to safely read user path strings
- `syscall_read` / `syscall_write` validate buffer pages via `is_page_mapped()`
- `syscall_ps`, `syscall_readdir`, `syscall_stat`, `syscall_getcwd`, `syscall_pipe` all use `copy_to_user()`

### 10.6 No per-process FD table ownership
- `files_open[]` holds FILE pointers but no reference counting
- `dup()` just copies the pointer (shared state between FDs)
- `close()` frees the FILE struct — other FDs pointing to it become dangling

### 10.7 `process_cleanup_child` only frees first 3 FDs
- Loop `for (int i = 0; i < 3; i++)` misses FDs 3..31
- Should iterate all MAX_FILES entries

### 10.8 Scheduler doesn't handle all-terminated state well
- If all processes are terminated, `current_process = NULL; sti; hlt` — halts forever
- No idle process or shell restart mechanism

---

## 11. Key Constants

| Constant | Value | Location |
|----------|-------|----------|
| `MAX_PROCESSES` | 10 | `proc/process.h` |
| `MAX_FILES` | 32 | `stdio.h` (libc) / `process.h` |
| `KERNEL_STACK_SIZE` | 8192 (2 pages) | `proc/process.h` |
| `USER_CODE_BASE` | 0x08048000 | `proc/process.h` |
| `USER_STACK_TOP` | 0xA0000000 | `proc/process.h` |
| `USER_STACK_PAGES` | 4 | `proc/process.h` |
| `KERNEL_VIRTUAL_BASE` | 0xC0000000 | `isr_c_gen.py`, `process_asm.asm` |
| `MAX_SYSCALLS` | 24 | `arch/syscalls.h` |
| `MAX_SYSCALL` | 23 | `syscalls_asm.asm` (0-indexed upper bound) |

---

## 12. Files to Modify for Ring 3 Hardening

| Task | Files |
|------|-------|
| User-mode page faults | `isr_c_gen.py` or ISR handler, `process.c` (terminate process) |
| fork() | `process.c`, `vmm.c` (deep copy page tables + user pages) |
| Priority/nice | `process.h` (add priority field), `process.c` (schedule logic) |
| Signals | `process.h` (signal masks, handler table), `syscalls.c` (sigaction, sigprocmask) |
| Safe user copy | `syscalls.c` (add copy_string_from_user with bounds checking) |
| FD reference counting | `vfs.h` (refcount on fs_node), `syscalls.c` (close/dup logic) |
| FD cleanup on exit | `process.c` process_cleanup_child (iterate all MAX_FILES) |
