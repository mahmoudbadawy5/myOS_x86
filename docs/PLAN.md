# Development Plan

| Phase | Status | Doc |
|-------|--------|-----|
| Phase 1: Kernel ABI Expansion | DONE | — |
| Phase 2: Minimal ANSI libc | DONE | [PHASE2_LIBC.md](PHASE2_LIBC.md) |
| Phase 3: Userspace Utilities | DONE | [PHASE3_UTILITIES.md](PHASE3_UTILITIES.md) |
| Phase 4: Ring 3 Process Hardening | PLANNED | [RING3_INFRA.md](RING3_INFRA.md) |
| Phase 5: Dynamic Library Support | PLANNED | (this file, below) |

---

## Phase 1: Kernel ABI Expansion (DONE)

### Completed
- [x] GDT user segments (ring 3 code/data)
- [x] Per-process address space (vmm_clone_directory, user page mappings)
- [x] Syscall ABI (regs pointer passed to C, return value written to saved EAX, DPL=3)
- [x] PCB, process table, create_process, schedule, switch_to_process
- [x] 24 syscalls: test0, test1, open, read, write, exit, yield, sbrk, spawn, wait, exec, dup, pipe, kill, getpid, lseek, readdir, stat, getcwd, chdir, close, mkdir, unlink, ps
- [x] Timer-driven preemption scheduler
- [x] Keyboard driver with Ctrl+C signal delivery
- [x] ELF loader with argv support
- [x] Pipe infrastructure
- [x] Process cleanup, orphan prevention
- [x] FAT12 read/write filesystem
- [x] ATA PIO disk driver
- [x] VFS mount points
- [x] User-mode page faults kill process (not kernel panic)
- [x] Safe user copy: copy_to_user, copy_from_user, is_page_mapped

---

## Phase 2: Minimal ANSI libc (DONE)

### Completed
- [x] string.h: 18 functions (memcpy..strtok)
- [x] ctype.h: 11 functions (isdigit..tolower)
- [x] stdio.h: printf, fprintf, sprintf, snprintf, vsnprintf, fopen, fclose, fread, fwrite, fseek, ftell, feof, ferror, fgets, fgetc, fputs, fputc, perror, puts, putchar, getchar
- [x] stdlib.h: malloc, free, realloc, atoi, atol, abs, labs
- [x] Syscall wrappers: 24 wrappers matching kernel syscalls
- [x] exit(int status) with correct signature

---

## Phase 3: Userspace Utilities (DONE)

### Completed
- [x] close syscall (#20) + sys_close wrapper + fclose fix
- [x] mkdir syscall (#21) + FAT12 mkdir with . and .. entries
- [x] unlink/rm syscall (#22) + FAT12 unlink with cluster chain free
- [x] ps syscall (#23) + ps app
- [x] Apps: ls, cat, echo, pwd, touch, mkdir, rm, ps, head, tail, wc, grep, hexdump, shell
- [x] Shell builtins: help, clear, cd, pwd
- [x] FAT12 create dedup, mount visibility, subdirectory create/mkdir/unlink
- [x] Bug fixes: exit signature, fat12_mkdir cleanup, process_cleanup_child

---

## Phase 4: Ring 3 Process Hardening (PLANNED)

See [RING3_INFRA.md](RING3_INFRA.md) for full current-state audit.

### 4A. FD Reference Counting

**Why:** `fork()` needs to share open files between parent and child. Currently `close()` frees the FILE struct, leaving other FDs pointing to it (dangling pointer).

**Design:**
- Add `uint32_t refcount` to `fs_node_t` in `vfs.h`
- `fopen()` — set `node->refcount = 1` (new files) or `node->refcount++` (reopening existing node)
- `syscall_close()` — `node->refcount--`; only call `close_fs()` and free FILE when refcount hits 0
- `fork()` — for each `files_open[i]`, increment `node->refcount`
- `process_cleanup_child()` — for each `files_open[i]`, decrement refcount, free FILE if 0

**Files:**
| File | Change |
|------|--------|
| `kernel/src/include/fs/vfs.h` | Add `refcount` field to `fs_node_t` |
| `kernel/src/arch/syscalls.c` | `syscall_close`: decrement refcount before freeing |
| `kernel/src/fs/fat12.c` | `fat12_make_node`: init refcount to 0 (not incremented until fopen) |
| `kernel/src/fs/initrd.c` | `initrd_finddir_fs`: init refcount on new nodes |
| `kernel/src/stdio/fopen.c` | Increment refcount when assigning node to FILE |

### 4B. Fix `process_cleanup_child` FD Loop

**Bug:** `for (int i = 0; i < 3; i++)` only frees FDs 0-2, misses 3-31.
**Fix:** Change to `for (int i = 0; i < MAX_FILES; i++)`.

**Files:**
| File | Change |
|------|--------|
| `kernel/src/proc/process.c` | Fix loop bound in `process_cleanup_child` |

### 4C. `fork()` Syscall (Standard Unix fork)

**Design:** Duplicate the calling process completely — same code, same data, same open files, same CWD. Child gets return value 0, parent gets child PID.

**How it works:**
1. Find free slot in `process_table`
2. Clone PCB (copy all fields, assign new PID)
3. `vmm_clone_directory()` — already exists, clones page directory + kernel page table entries
4. **Deep-copy user pages**: iterate parent's VMA list, for each user page:
   - Allocate new physical page
   - Copy contents from parent's page to new page
   - Map into child's page directory at the same virtual address with PAGE_USER
5. Copy `files_open[]` — for each open FD, increment `fs_node_t->refcount` (shared FILE structs)
6. Copy CWD string
7. Set child's `regs = parent->regs` (same instruction pointer, stack, registers)
8. Child's `regs.eax = 0` (fork return value in child)
9. Parent's return value = child PID (written to saved EAX in trap frame at offset 44)
10. Child state = `PROCESS_STATE_READY`

**Syscall number:** #24

**Files:**
| File | Change |
|------|--------|
| `kernel/src/proc/process.c` | `fork_process(pcb_t *parent, struct regs *regs)` |
| `kernel/src/arch/syscalls.c` | `syscall_fork` handler |
| `kernel/src/arch/syscalls_asm.asm` | Bump `MAX_SYSCALL` to 24 |
| `kernel/src/include/arch/syscalls.h` | Bump `MAX_SYSCALLS` to 25 |
| `libc/src/syscalls.c` | `int sys_fork(void)` wrapper |
| `libc/src/include/syscalls.h` | Declare `sys_fork` |
| `apps/test_fork.c` | Test app (optional) |

**Implementation notes:**
- `vmm_clone_directory()` currently only copies kernel page table entries (upper half). For fork, we need to also copy user page table entries (lower half) and allocate new physical pages for each present user page.
- The parent's VMA list must be deep-copied (malloc new vma_t nodes, copy fields).
- Child inherits parent's signal_pending = 0, num_children = 0.
- Child's kernel stack is freshly allocated (separate from parent).

### 4D. Idle Process

**Problem:** If all processes terminate, `schedule()` sets `current_process = NULL; sti; hlt` — halts forever.

**Solution:** Create a special idle process at boot that runs a `hlt` loop. It's always READY and only scheduled when no other process is available.

**Design:**
- In `init_multitasking()`, create a minimal PCB for PID 0 with:
  - `state = PROCESS_STATE_READY`
  - `regs.eip` = address of idle loop function (kernel-space, mapped into process)
  - `regs.cs = 0x08` (kernel code segment — idle runs in ring 0)
  - `regs.eflags = 0x202` (IF=1)
- Idle loop: `for (;;) { __asm__ __volatile__("hlt"); }`
- Scheduler fallback: if no READY/NEW process found, pick idle (PID 0)

**Files:**
| File | Change |
|------|--------|
| `kernel/src/proc/process.c` | Add `init_idle_process()`, modify `schedule()` fallback |
| `kernel/src/proc/process_asm.asm` | Idle process entry point (optional, can use C function) |

### Execution order for Phase 4

1. **4B** — Fix `process_cleanup_child` FD loop (trivial, do first)
2. **4A** — FD refcounting (prerequisite for fork)
3. **4C** — `fork()` syscall
4. **4D** — Idle process
5. Build, test, commit

---

## Phase 5: Dynamic Library Support (PLANNED)

### Design Decisions

1. **Library address determined at load time** — not hardcoded. Kernel picks a free virtual address range above app code but below stack. Allows multiple libraries in the future.
2. **Eager binding** — all symbols resolved at load time. No PLT stubs, no lazy resolution, no runtime linker.
3. **ELF contains DT_NEEDED** — each app's ELF dynamic section lists required libraries. Kernel finds them in a `lib/` folder on the filesystem.
4. **Static fallback** — apps that don't need libc (raw syscalls) can still link statically.

### Architecture Overview

```
┌─────────────────────────────────────────────┐
│  Process Virtual Address Space              │
│                                             │
│  0x08048000 ┌──────────────┐                │
│             │ App .text     │                │
│             │ App .data     │                │
│             │ App GOT       │ ← patched by   │
│  0x40000000 ├──────────────┤   kernel loader │
│             │ libc.so       │ ← shared, R/X  │
│             │ (read-only)   │   across all    │
│             │               │   processes     │
│  0xA0000000 ├──────────────┤                │
│             │ User Stack    │                │
│             └──────────────┘                │
└─────────────────────────────────────────────┘
```

### 5A. Compile libc as Shared Object

**Goal:** Produce `lib/libc.so` — a position-independent ELF shared object.

**Build changes:**
- New linker script: `libc/libc_so.ld` — base address placeholder (overridden by kernel at load time), produces `ET_DYN` ELF
- New Makefile target: `make libc.so` — compiles with `-fPIC -shared -nostdlib`
- The `.text` and `.data` sections go into a single `PT_LOAD` segment
- Export a `.dynsym` symbol table with all public functions
- Keep existing `build/libc.o` (static) for kernel and static apps

**New files:**
| File | Purpose |
|------|---------|
| `libc/libc_so.ld` | Linker script for shared object |
| `lib/` | Directory for shared libraries (created at build time) |

**Makefile changes:**
| File | Change |
|------|--------|
| `libc/Makefile` | Add `build/libc.so` target |
| `Makefile` | Add `lib/libc.so` to initrd, add `lib/` directory |

### 5B. ELF Loader: Dynamic Section Parsing

**Goal:** Extend `load_elf()` to handle `PT_DYNAMIC` segments and resolve symbols.

**What the loader needs to parse:**

The `PT_DYNAMIC` segment contains an array of `Elf32_Dyn` entries:
```c
typedef struct {
    Elf32_Sword d_tag;    // type
    union {
        Elf32_Word d_val;
        Elf32_Addr d_ptr;
    } d_un;
} Elf32_Dyn;
```

Key tags to handle:
| Tag | Purpose |
|-----|---------|
| `DT_NEEDED` | Index into dynstr: library filename (e.g., "libc.so") |
| `DT_SYMTAB` | Pointer to `.dynsym` section |
| `DT_STRTAB` | Pointer to `.dynstr` section (string table) |
| `DT_STRSZ` | Size of dynstr |
| `DT_JMPREL` | Pointer to `.rel.plt` (PLT relocations) |
| `DT_PLTRELSZ` | Size of `.rel.plt` in bytes |
| `DT_REL` | Pointer to `.rel.dyn` (dynamic relocations) |
| `DT_RELSZ` | Size of `.rel.dyn` |
| `DT_NULL` | End of dynamic section |

**Relocation types (i386):**
| Type | Value | Meaning |
|------|-------|---------|
| `R_386_NONE` | 0 | No relocation |
| `R_386_32` | 1 | S + A (symbol value + addend) |
| `R_386_PC32` | 2 | S + A - P (PC-relative) |
| `R_386_GOT32` | 3 | G + A (GOT entry offset) |
| `R_386_PLT32` | 4 | L + A - P (PLT entry) |
| `R_386_COPY` | 5 | Copy symbol at runtime |
| `R_386_JMP_SLOT` | 7 | S (resolve to symbol value) — used by GOT.PLT |
| `R_386_RELATIVE` | 8 | B + A (base address + addend) |
| `R_386_GLOB_DAT` | 6 | S (resolve to symbol value) — used by GOT |

For eager binding, we handle `R_386_JMP_SLOT` and `R_386_GLOB_DAT` by looking up the symbol in the loaded library and patching the GOT entry.

**Loader algorithm:**
```
load_elf(proc, path):
    // ... existing PT_LOAD code ...

    // Find PT_DYNAMIC segment
    for each phdr:
        if phdr.p_type == PT_DYNAMIC:
            dynamic_addr = phdr.p_vaddr
            dynamic_size = phdr.p_filesz

    if no PT_DYNAMIC found:
        return (static binary, proceed as before)

    // Parse dynamic section
    dyn = (Elf32_Dyn *)dynamic_addr
    for each dyn entry:
        switch dyn.d_tag:
            case DT_NEEDED: dynstr_idx = dyn.d_un.d_val; lib_name = &dynstr[dynstr_idx]
            case DT_SYMTAB: symtab = dyn.d_un.d_ptr
            case DT_STRTAB: strtab = dyn.d_un.d_ptr
            case DT_JMPREL: jmprel = dyn.d_un.d_ptr; pltrelsz = next entry
            case DT_REL: rel = dyn.d_un.d_ptr; relsz = next entry

    // Load each DT_NEEDED library
    for each needed library name:
        find lib in lib/ folder on filesystem
        load library ELF (PT_LOAD segments) at a free virtual address
        store: lib_base_addr, lib_symtab, lib_strtab

    // Resolve relocations
    for each entry in jmprel (PLT relocations):
        sym_idx = entry.r_info >> 8
        type = entry.r_info & 0xFF
        symbol = &symtab[sym_idx]
        name = &strtab[symbol->st_name]

        // Look up symbol in loaded libraries
        for each loaded library:
            if symbol exists in library's symtab:
                resolved_addr = library_base + symbol->st_value
                break

        // Patch GOT entry
        *(uint32_t *)entry.r_offset = resolved_addr

    // Same for .rel.dyn entries (R_386_GLOB_DAT, R_386_RELATIVE)
```

**New ELF types needed in `elf.h`:**
```c
#define ET_DYN      3   // Shared object
#define PT_DYNAMIC  2   // Dynamic section
#define PT_INTERP   3   // Interpreter (not used but good to define)

#define DT_NULL     0
#define DT_NEEDED   1
#define DT_STRTAB   5
#define DT_SYMTAB   6
#define DT_STRSZ   10
#define DT_PLTRELSZ 2
#define DT_JMPREL  23
#define DT_REL     17
#define DT_RELSZ   18

#define R_386_NONE    0
#define R_386_32      1
#define R_386_PC32    2
#define R_386_GOT32   3
#define R_386_PLT32   4
#define R_386_COPY    5
#define R_386_GLOB_DAT 6
#define R_386_JMP_SLOT 7
#define R_386_RELATIVE 8

typedef struct {
    Elf32_Sword d_tag;
    union { Elf32_Word d_val; Elf32_Addr d_ptr; } d_un;
} Elf32_Dyn;

typedef struct {
    Elf32_Addr r_offset;
    Elf32_Word r_info;
} Elf32_Rel;

typedef struct {
    Elf32_Word st_name;
    Elf32_Addr st_value;
    Elf32_Word st_size;
    uint8_t    st_info;
    uint8_t    st_other;
    Elf32_Half st_shndx;
} Elf32_Sym;
```

**Files:**
| File | Change |
|------|--------|
| `kernel/src/include/proc/elf.h` | Add ELF dynamic types, structs, defines |
| `kernel/src/proc/elf.c` | Extend `load_elf()` to parse PT_DYNAMIC, load libraries, resolve relocations |
| `kernel/src/mem/vmm.c` | Add `find_free_address()` to find unmapped region for library loading |

### 5C. Library Address Space Layout

**Goal:** Determine where to map shared libraries at load time.

**Strategy:**
- App code: `0x08048000` - `0x0FFFFFFF` (existing)
- Shared libraries: `0x20000000` - `0x3FFFFFFF` (new region, grows upward)
- User stack: `0xA0000000` - `0xBFFFFFFF` (existing, grows downward)

**Implementation:**
- Add a `lib_base` field to PCB: tracks where the next library would be mapped
- Initialize to `0x20000000` on process creation
- Each library is mapped at `lib_base`, then `lib_base += ALIGN_PAGE_UP(lib_size)`
- Multiple libraries (libc.so, libm.so, etc.) map at consecutive addresses

**Files:**
| File | Change |
|------|--------|
| `kernel/src/include/proc/process.h` | Add `lib_base` field to PCB |
| `kernel/src/proc/process.c` | Initialize `lib_base = 0x20000000` in `create_process` |

### 5D. Apps: PIC Compilation + Dynamic Linker Script

**Goal:** Compile apps as position-independent executables that reference the shared library.

**New linker script:** `apps/linker_dynamic.ld`

The key difference from `linker.ld`:
- App code at `0x08048000` (same)
- Contains `.dynamic`, `.dynsym`, `.dynstr`, `.rel.plt`, `.got.plt` sections
- External symbols are unresolved — the kernel loader patches them

**Compilation workflow:**
```bash
# Compile app as PIC
i686-elf-gcc -fPIC -c -o app.o app.c

# Link with dynamic linker script
# -N: omit .comment section
# -shared: allow unresolved symbols (they'll be resolved by kernel)
i686-elf-ld -T linker_dynamic.ld -N -o app_dynamic.bin app.o
```

Note: We don't link against libc.so at link time. The app's `.rel.plt` entries contain symbol names that the kernel resolves at load time. This is simpler than traditional dynamic linking where the linker creates the PLT/GOT — here the kernel does it.

**Alternative (simpler):** Use the existing `linker.ld` for apps but add `-shared` flag so the linker creates dynamic sections. The kernel then patches everything.

**Files:**
| File | Change |
|------|--------|
| `apps/linker_dynamic.ld` | New linker script for dynamically-linked apps |
| `apps/Makefile` | Add dynamic linking build rules |

### 5E. Kernel: Load Libraries at Boot

**Goal:** Load shared library files once at boot and keep them in kernel memory for quick mapping into processes.

**Design:**
- At boot (after FAT12/initrd init), scan `lib/` folder
- Load each `.so` file into a kernel buffer (one-time read)
- Store in a linked list: `{ char name[64]; uint8_t *data; uint32_t size; }`
- When `create_process` or `load_program` encounters a `PT_DYNAMIC` app:
  - For each `DT_NEEDED` library, find it in the loaded library list
  - Map its pages (read-only, PAGE_USER) at the process's `lib_base` address
  - Use the in-memory copy (no disk I/O needed per process creation)

**Files:**
| File | Change |
|------|--------|
| `kernel/src/fs/vfs.c` | Add `load_shared_libs()` called at boot |
| `kernel/src/include/fs/vfs.h` | Declare `shared_lib_t` struct and `load_shared_libs()` |
| `kernel/src/kmain.c` | Call `load_shared_libs()` after filesystem init |

### 5F. Apps: Build Both Modes

Keep the ability to build apps in two modes:
- **Static** (current): `make` in `apps/` → links `libc.o` into each binary
- **Dynamic** (new): `make dynamic` in `apps/` → produces smaller binaries that reference `lib/libc.so`

For the transition period, keep all existing apps static. Convert one or two (e.g., `cat`, `ls`) to dynamic as proof of concept.

**Files:**
| File | Change |
|------|--------|
| `apps/Makefile` | Add `make dynamic` target |
| `Makefile` | Add `lib/` to initrd |

### 5G. Libc: Remove crt0 from Shared Object

The CRT startup (`crt0.asm`) should NOT be in `libc.so` — it's app-specific. Each app has its own `_start` entry point.

**Changes:**
- `libc/src/crt0.asm` stays in the static `libc.o` only
- `libc_so.ld` excludes `crt0.o` from the shared object
- Dynamic apps get `_start` from a new `crt0_dyn.asm` that:
  1. Pops argc, sets up argv (same as current crt0)
  2. Calls `main`
  3. Calls `sys_exit`

This is actually the same as the current crt0 — it just doesn't go into libc.so.

**Files:**
| File | Change |
|------|--------|
| `libc/libc_so.ld` | Exclude crt0 from shared object |

### Execution order for Phase 5

1. **5A** — Compile libc as shared object
2. **5E** — Kernel: load libraries at boot, store in memory
3. **5B** — ELF loader: parse dynamic sections, resolve symbols, patch GOT
4. **5C** — Library address space layout
5. **5D** — Apps: PIC compilation + dynamic linker script
6. **5G** — Libc: exclude crt0 from shared object
7. **5F** — Build system: both static and dynamic modes
8. Convert 2-3 apps to dynamic as proof of concept
9. Build, test all apps in QEMU
10. Commit

---

## Execution Order: Phase 4 + Phase 5

1. Phase 4B — Fix process_cleanup_child FD loop
2. Phase 4A — FD reference counting
3. Phase 4C — fork() syscall
4. Phase 4D — Idle process
5. Phase 4: build, test, commit
6. Phase 5A — Compile libc as shared object
7. Phase 5E — Kernel: load libraries at boot
8. Phase 5B — ELF loader: dynamic section parsing + symbol resolution
9. Phase 5C — Library address space layout
10. Phase 5D — Apps: PIC compilation + dynamic linker script
11. Phase 5G — Libc: exclude crt0 from shared object
12. Phase 5F — Build system for both modes
13. Phase 5: convert proof-of-concept apps, test, commit
