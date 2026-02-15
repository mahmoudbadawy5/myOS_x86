# Plan: User Space and Task Scheduling

This document outlines how to add **user space** (ring 3) execution and **task scheduling** to myOS_x86, based on the current clean state of the repo.

---

## Current State Summary

| Component | Status |
|-----------|--------|
| Boot | Multiboot, higher-half kernel at 0xC0000000, paging |
| Memory | Physical bitmap allocator, kernel paging, malloc |
| GDT | 3 entries only: null, kernel code (0x08), kernel data (0x10). **No user segments** |
| IDT/ISR/IRQ | Installed; timer counts ticks only |
| VFS/initrd | Working; `get_node`, `finddir_fs`, `read_fs` |
| Syscalls | INT 0x80 handler present; **does not pass regs to C**, **does not return value to user**; gate DPL=0 (kernel-only) |
| virt_mem | `map_address`, `get_page`, `set_page_dir`; **no** `vmm_clone_directory` / `vmm_get_directory`; **no** user page flag in mappings |
| Processes | **None** – no PCB, no process table, no scheduler |
| kmain | Loads `echo.bin` at **0** (identity), runs in kernel; then infinite loop |

User programs (e.g. `echo.bin`) use syscalls (`read`/`print` from libc) but are currently run in kernel context; to run them in ring 3 we need the pieces below.

---

## Part 1: User Space

### 1.1 GDT – user segments

**File:** [kernel/src/arch/gdt.c](kernel/src/arch/gdt.c)

- Add two entries after the existing three:
  - **User code:** base 0, limit 0xFFFFFFFF, access 0xFA (ring 3, code), gran 0xCF → selector **0x1B** (index 3, RPL 3).
  - **User data:** base 0, limit 0xFFFFFFFF, access 0xF2 (ring 3, data), gran 0xCF → selector **0x23** (index 4, RPL 3).
- Update `gp.limit` to cover 5 entries.

So the kernel keeps 0x08 (code) and 0x10 (data); user code uses 0x1B and user data 0x23.

### 1.2 Per-process address space

**Files:** [kernel/src/mem/virt_mem.c](kernel/src/mem/virt_mem.c), [kernel/src/include/mem/virt_mem.h](kernel/src/include/mem/virt_mem.h)

- **`vmm_get_directory(void)`**  
  Return current page directory as physical address (e.g. `(uint32_t)cur_page_dir - KERNEL_VIRTUAL_BASE`). Needed so the kernel can save/restore CR3 per process.

- **`vmm_clone_directory(void)`**  
  Allocate a new page directory; copy kernel-half entries (e.g. indices >= `KERNEL_PAGE_NUMBER`) from the current directory so kernel mappings are shared; leave lower indices empty for process-specific user mappings. Return physical address of the new directory.

- **User mappings**  
  When mapping pages for user space (e.g. code at 0x40000000, stack at 0xE0000000), set the **PAGE_USER** bit (0x4) in the page table entry so the page is accessible in ring 3. Existing `map_address` only sets PRESENT and RW; either add a variant (e.g. `map_address_user`) or an extra parameter so user pages get USER set.

- Optionally export **`set_page_dir`** in the header so process/scheduler code can switch address spaces.

### 1.3 Syscall ABI (so user code gets regs and return value)

**File:** [kernel/src/arch/syscalls_asm.asm](kernel/src/arch/syscalls_asm.asm)

- After pushing all saved registers, the stack pointer points at a structure matching `syscall_regs_t`. **Pass this pointer** to the C handler (e.g. push ESP; call handler; add ESP, 4). C handlers should take `syscall_regs_t *regs` and read arguments from it.
- **Return value:** the C handler returns in EAX. Before restoring registers and `iretd`, **write this EAX into the saved “eax” slot** in the saved regs (the slot that will be popped into EAX when we restore). Then when we `iretd`, userland sees the syscall return value in EAX.

**File:** [kernel/src/arch/syscalls.c](kernel/src/arch/syscalls.c)

- Change handler type to `int32_t (*)(syscall_regs_t *regs)` and use `regs->ebx`, etc. (and `regs->eax` for return slot if needed). Update `init_syscalls` so the IDT gate for 0x80 uses **0xEE** (or equivalent) so **DPL=3** and user code can execute `int 0x80`.

### 1.4 Allow user to call INT 0x80

**File:** [kernel/src/arch/syscalls.c](kernel/src/arch/syscalls.c)

- Replace `idt_set_gate(0x80, ..., 0x8E)` with flags **0xEE** so the interrupt gate is callable from ring 3 (DPL=3). Without this, `int 0x80` from user will cause a GPF.

---

## Part 2: Task Scheduling

### 2.1 Process control block (PCB) and process table

**New files:** e.g. `kernel/src/include/process.h`, `kernel/src/user/process.c`, `kernel/src/user/process_asm.asm` (or under `arch/` if you prefer).

- **`registers_t`** – saved CPU state: eax, ebx, ecx, edx, esi, edi, ebp, esp, eip, eflags, cr3; and **cs, ds, es, fs, gs, ss** (so segment regs are saved/restored for user mode).
- **`pcb_t`** – at least: pid, state (e.g. READY, RUNNING, BLOCKED, TERMINATED), `registers_t regs`, **kernel_stack_top** (so each process has its own kernel stack).
- **Process table** – fixed-size array of `pcb_t` (e.g. MAX_PROCESSES = 10); current process index; next_pid.
- **States:** READY, RUNNING, BLOCKED, TERMINATED (and optionally IDLE for the idle task).

### 2.2 Per-process kernel stack

- When creating a process, allocate a small kernel stack (e.g. 1–2 pages) and store its **top** (high address) in `pcb_t.kernel_stack_top`.
- On context switch **to** this process, set kernel ESP to that top (or to a region just below it for a synthetic iret frame when the process has never run).
- On context switch **from** this process, the current kernel ESP is saved in the PCB (as part of saved state) so we can restore it when we switch back.

### 2.3 Creating a process (load user program)

**Logic (in `create_process` or similar):**

1. Take a path (e.g. `"echo.bin"`) and resolve it via VFS (`finddir_fs(root_dir, name)` or `get_node("/echo.bin", root_dir)`).
2. Allocate a new page directory with `vmm_clone_directory()` and make it current (`set_page_dir`).
3. Map **user code** at a fixed VA (e.g. **0x40000000**): allocate enough physical pages, map with **PAGE_USER | PAGE_RW** (code can be RX only if you prefer; for simplicity RW is fine). Load the binary from the initrd into that VA with `read_fs`.
4. Map **user stack** at a high user VA (e.g. **0xE0000000**), a few pages, with PAGE_USER | PAGE_RW. Initial user ESP = top of that region.
5. Save in the PCB: **eip** = 0x40000000 (or your chosen entry), **esp** = user stack top, **eflags** = 0x202 (IF), **cr3** = physical page dir, **cs** = 0x1B, **ds** = **es** = **fs** = **gs** = **ss** = 0x23. Set **kernel_stack_top** to the allocated kernel stack. Mark state READY.
6. **First-run iret frame:** For a process that has never run, the “saved” kernel stack must look like the stack after an interrupt from user mode. So when we switch to this process the first time, we will load ESP with the address of a **synthetic iret frame** on its kernel stack: [user SS] [user ESP] [eflags] [user CS] [user EIP]. Then the switch routine restores other regs and does `iretd`, and the CPU drops to ring 3 at the process entry point. So either:
   - Reserve space on the process’s kernel stack and build this frame at create time; store in the PCB the pointer to this frame (as the “saved ESP” for the first run), or
   - Have the scheduler detect “first run” and set ESP to a pre-built frame on the process kernel stack, then iret.
7. Restore the **previous** page directory (kernel’s or previously current process’s) before returning from `create_process`.

### 2.4 Scheduler and context switch

**Timer:** In [kernel/src/drivers/timer.c](kernel/src/drivers/timer.c), call **`schedule()`** from the timer handler so the scheduler runs on every tick (or every N ticks).

**`schedule()` (C):**

- If there is a **current** process (e.g. `current_process_index` valid and state RUNNING), mark it READY and save nothing here (saving is done in asm when we switch).
- Pick the **next** runnable process (e.g. round-robin over READY/RUNNING, skipping TERMINATED/BLOCKED). If none, either run an **idle** task or return (no switch).
- Call **`switch_to_process(&process_table[next_index])`** (or pass next PCB pointer). The asm will save current state into the **current** PCB (if any), then load next PCB and resume it.

**`switch_to_process(pcb_t *next)` (asm):**

- Save current state: push all general-purpose and segment regs; save this ESP in the **current** PCB’s saved kernel stack pointer (you need a way to get “current PCB” – e.g. global pointer or passed as second argument). Save CR3 if you want to restore it later (or it’s already in current PCB).
- Load **next** PCB: load CR3 from `next->regs.cr3`; load kernel ESP from `next->kernel_stack_top` (or from next’s saved ESP if the process was already running before). For **first run**, the “saved” kernel ESP should point to the synthetic iret frame.
- Restore segment regs and general-purpose regs from `next->regs` (or from the stack if you store the state on the kernel stack in the same order as the iret frame).
- **iretd** – the CPU will either return to the next process’s kernel path (if it was in kernel) or to user mode (if the stack holds the iret frame with user CS/EIP/SS/ESP/eflags).

Design detail: when the **current** process is interrupted by the timer, the CPU has already pushed SS, ESP, eflags, CS, EIP. The IRQ handler then pushes more regs. So the “saved state” we need is the kernel stack pointer at the point we want to restore. When we switch to another process, we set ESP to that process’s saved kernel stack pointer and then pop/iret. So the PCB should store the **kernel stack pointer** (and for first run that stack holds the synthetic iret frame). When saving the current process, we store the current ESP in the current PCB.

**Idle:** Either add a special “idle” PCB whose “entry” is a small asm loop (e.g. `sti; hlt; jmp`), or have `schedule()` when there is no current process (e.g. before any process has run) not call `switch_to_process` and just return so the kernel loop keeps running until the first timer-driven switch. For the first switch “from” kernel (no process), the “current” PCB is invalid – so the asm must handle “current == NULL” by not saving, and only loading the next process and iret’ing to it.

### 2.5 Process lookup by name

In [kernel/src/kmain.c](kernel/src/kmain.c), instead of manually loading `echo.bin` at 0 and calling it, call e.g. **`create_process("echo.bin")`** (or `"echo.bin"` as used in the initrd). Ensure the initrd contains the binary with that name (your Makefile already copies `apps/*.bin` into initrd). So create_process must look up by the name that appears in the initrd (e.g. `finddir_fs(root_dir, "echo.bin")` or `get_node("/echo.bin", root_dir)`).

### 2.6 Build and link

- Add `process.c` and `process_asm.asm` to the kernel build (e.g. in [kernel/Makefile](kernel/Makefile)).
- Link `schedule` from the timer handler; ensure `process.h` is included where needed and that the scheduler is initialized (e.g. `init_multitasking()`) and that at least one process is created (e.g. `create_process("echo.bin")`) before enabling interrupts and entering the main loop.

---

## Part 3: Optional Improvements

- **exit() syscall:** Process calls exit; kernel marks it TERMINATED; scheduler skips it; eventually we could reuse the PCB.
- **yield() syscall:** Voluntary call to `schedule()` so the process gives up the CPU.
- **Scheduler skip:** In `schedule()`, when iterating for the next process, skip entries with state TERMINATED (and BLOCKED if you use it).

---

## Implementation Order (suggested)

1. **GDT user segments** – required for ring 3.
2. **virt_mem:** `vmm_get_directory`, `vmm_clone_directory`, user-page mapping (PAGE_USER).
3. **Syscall ABI:** Pass regs pointer to C; write return value to saved EAX; set IDT 0x80 to DPL=3 (0xEE).
4. **PCB, process table, create_process:** Data structures, create one process with code at 0x40000000, stack at 0xE0000000, per-process page dir, segment regs and eip/esp set; **per-process kernel stack** and **first-run iret frame**.
5. **switch_to_process asm:** Save current (if any), load next (CR3, kernel ESP), restore, iretd.
6. **schedule():** Called from timer; round-robin; handle “no current” on first run.
7. **kmain:** Init multitasking, create_process("echo.bin"), sti, infinite loop (scheduler will switch to echo).
8. **Optional:** exit, yield, skip TERMINATED in scheduler.

This order gets a single user process running in ring 3 with working syscalls; then you can add more processes and refine the scheduler.
