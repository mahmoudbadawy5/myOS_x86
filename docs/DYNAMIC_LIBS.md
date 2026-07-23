# Dynamic Library Support — Implementation Plan

## Overview

Add support for ELF shared objects so libc (and future libraries) are compiled once
and shared across all processes. Apps become smaller, and adding new libraries doesn't
require recompiling every app.

## Current State

- libc is compiled once into `libc.o` (relocatable), then `ld -r` merges it into every app
- Each app carries its own copy of libc code (~20-40KB per app)
- ELF loader only handles `PT_LOAD` segments, no dynamic sections
- No shared library infrastructure anywhere

## Target State

- libc compiled once as `lib/libc.so` (ELF shared object, `-fPIC -shared`)
- Kernel loads `libc.so` once at boot, maps it read-only into every process
- Apps are compiled as PIC executables with a GOT (Global Offset Table)
- ELF loader resolves all symbols at load time (eager binding)
- Apps that don't use libc can still link statically

## Address Space Layout

```
0x00000000 ┌──────────────────┐
           │ (unmapped)       │
0x08048000 ├──────────────────┤
           │ App .text        │
           │ App .data        │
           │ App GOT          │
0x20000000 ├──────────────────┤ ← lib_base (per-process, grows up)
           │ libc.so .text    │
           │ libc.so .data    │
           │ (shared, R/X)    │
           │                  │
           │ (next lib here)  │
0xA0000000 ├──────────────────┤
           │ User Stack       │
           │ (grows down)     │
0xC0000000 ├──────────────────┤
           │ Kernel           │
           └──────────────────┘
```

Library base address (`0x20000000`) is the start of the reserved range. The kernel
allocates from this range as libraries are loaded. Multiple libraries map at
consecutive addresses.

## ELF Structures

### Dynamic Section Entry

```c
typedef struct {
    Elf32_Sword d_tag;      // Type (DT_NEEDED, DT_SYMTAB, etc.)
    union {
        Elf32_Word d_val;   // Integer value
        Elf32_Addr d_ptr;   // Virtual address
    } d_un;
} Elf32_Dyn;
```

### Symbol Table Entry

```c
typedef struct {
    Elf32_Word st_name;     // Index into dynstr
    Elf32_Addr st_value;    // Symbol value (offset from library base)
    Elf32_Word st_size;     // Size of symbol
    uint8_t    st_info;     // Bind (high 4 bits) + Type (low 4 bits)
    uint8_t    st_other;    // Visibility
    Elf32_Half st_shndx;    // Section index
} Elf32_Sym;
```

### Relocation Entry

```c
typedef struct {
    Elf32_Addr r_offset;    // Address to patch (GOT entry address)
    Elf32_Word r_info;      // Symbol index (>>8) + Relocation type (&0xFF)
} Elf32_Rel;
```

### Key Dynamic Tags

| Tag | Value | Meaning |
|-----|-------|---------|
| `DT_NULL` | 0 | End of dynamic section |
| `DT_NEEDED` | 1 | Index into dynstr: shared library name |
| `DT_STRTAB` | 5 | Virtual address of `.dynstr` |
| `DT_SYMTAB` | 6 | Virtual address of `.dynsym` |
| `DT_STRSZ` | 10 | Size of `.dynstr` in bytes |
| `DT_PLTRELSZ` | 2 | Size of `.rel.plt` in bytes |
| `DT_JMPREL` | 23 | Virtual address of `.rel.plt` |
| `DT_REL` | 17 | Virtual address of `.rel.dyn` |
| `DT_RELSZ` | 18 | Size of `.rel.dyn` in bytes |

### Relocation Types (i386)

| Type | Value | Formula | Used for |
|------|-------|---------|----------|
| `R_386_NONE` | 0 | — | No relocation |
| `R_386_32` | 1 | `S + A` | Absolute address |
| `R_386_PC32` | 2 | `S + A - P` | PC-relative call |
| `R_386_GLOB_DAT` | 6 | `S` | GOT data entry |
| `R_386_JMP_SLOT` | 7 | `S` | GOT PLT entry (lazy, but we resolve eagerly) |
| `R_386_RELATIVE` | 8 | `B + A` | Base-relative (PIC) |

Where: S = symbol value, A = addend, P = place (address being relocated), B = base address.

## Loader Algorithm

```
load_elf(proc, path):
    // 1. Load PT_LOAD segments (existing code)
    for each program header:
        if p_type == PT_LOAD:
            alloc_mem_area(proc, p_vaddr, p_memsz, flags)
            read data from file into p_vaddr

    // 2. Find PT_DYNAMIC segment
    for each program header:
        if p_type == PT_DYNAMIC:
            dynamic_vaddr = p_vaddr
            break

    if not found:
        // Static binary — done
        return 0

    // 3. Parse dynamic section
    dyn = (Elf32_Dyn *)dynamic_vaddr
    while dyn->d_tag != DT_NULL:
        switch dyn->d_tag:
            DT_NEEDED: lib_names[num_libs++] = &dynstr[dyn->d_un.d_val]
            DT_SYMTAB: symtab = dyn->d_un.d_ptr
            DT_STRTAB: strtab = dyn->d_un.d_ptr
            DT_JMPREL: jmprel = dyn->d_un.d_ptr
            DT_PLTRELSZ: pltrelsz = dyn->d_un.d_val
            DT_REL: reltab = dyn->d_un.d_ptr
            DT_RELSZ: relsz = dyn->d_un.d_val
        dyn++

    // 4. Load each DT_NEEDED library
    for i = 0 to num_libs:
        lib = find_shared_lib(lib_names[i])  // from boot-loaded list
        if not lib:
            ERROR("missing library: %s", lib_names[i])
            return -1

        lib_addr = proc->lib_base
        map_shared_lib(proc, lib, lib_addr)
        proc->lib_base += ALIGN_PAGE_UP(lib->size)

        // Store library info for symbol resolution
        loaded_libs[i].base = lib_addr
        loaded_libs[i].symtab = lib_addr + (lib->dyn_symtab_offset)
        loaded_libs[i].strtab = lib_addr + (lib->dyn_strtab_offset)

    // 5. Resolve relocations
    // Process .rel.plt (PLT relocations — function calls via GOT)
    num_rels = pltrelsz / sizeof(Elf32_Rel);
    for i = 0 to num_rels:
        rel = &jmprel[i]
        sym_idx = rel->r_info >> 8
        rel_type = rel->r_info & 0xFF

        symbol = &symtab[sym_idx]
        name = &strtab[symbol->st_name]

        // Find which library exports this symbol
        for each loaded_lib:
            lib_sym = lookup_symbol(loaded_lib, name)
            if found:
                resolved = loaded_lib.base + lib_sym.st_value
                break

        if not found:
            ERROR("unresolved symbol: %s", name)
            return -1

        // Patch GOT entry
        *(uint32_t *)rel->r_offset = resolved

    // Process .rel.dyn (data relocations — GOT data entries)
    // Similar to above, handle R_386_GLOB_DAT and R_386_RELATIVE

    return 0
```

## Symbol Resolution

For eager binding, the kernel needs to look up symbols by name. This requires:

1. **Loading library symbol tables at boot** — when `load_shared_libs()` reads each .so file, also parse its `.dynsym` and `.dynstr` sections and store them in the `shared_lib_t` struct.

2. **Linear search** — for each unresolved symbol, iterate each loaded library's symbol table. This is O(n*m) but fine for a hobby OS with few symbols.

3. **Symbol binding** — only resolve `STB_GLOBAL` (1) and `STB_WEAK` (2) symbols. `STB_LOCAL` (0) symbols are not exported.

```c
// Check if symbol is exported
uint8_t bind = ELF32_ST_BIND(sym->st_info);
if (bind != STB_GLOBAL && bind != STB_WEAK)
    continue;  // skip local symbols

// Match by name
if (strcmp(&lib_strtab[sym->st_name], name) == 0)
    return sym;
```

## Shared Library Format

At boot, the kernel reads `.so` files and stores them as:

```c
typedef struct shared_lib {
    char name[64];              // e.g., "libc.so"
    uint8_t *data;              // raw ELF file contents
    uint32_t size;              // file size
    // Parsed at load time (lazy):
    uint32_t dyn_symtab_offset; // offset to .dynsym in data
    uint32_t dyn_strtab_offset; // offset to .dynstr in data
    uint32_t dyn_strtab_size;
    struct shared_lib *next;
} shared_lib_t;
```

## Shared Library Linker Script

The `libc_so.ld` script produces an ELF shared object with:

```
OUTPUT_FORMAT(elf32-i386)

BASE = 0x00000000;  /* Base address (overridden by kernel) */

SECTIONS
{
    . = BASE;

    .text : ALIGN(4K) {
        *(.text*)
    }

    .rodata : ALIGN(4K) {
        *(.rodata*)
    }

    .data : ALIGN(4K) {
        *(.data*)
    }

    .bss : ALIGN(4K) {
        *(.bss*)
        *(COMMON)
    }

    .dynsym : ALIGN(4K) {
        *(.dynsym)
    }

    .dynstr : ALIGN(4K) {
        *(.dynstr)
    }

    .dynamic : ALIGN(4K) {
        *(.dynamic)
    }

    /DISCARD/ : {
        *(.eh_frame)
        *(.comment)
        *(.note*)
        *(.got*)
        *(.plt*)
    }
}
```

Note: `.got` and `.plt` are DISCARDED because the shared library itself uses
relative addressing (PIC). The GOT exists in each app, not in the library.

## Apps Dynamic Linker Script

```ld
OUTPUT_FORMAT(elf32-i386)
ENTRY(_start)

SECTIONS
{
    . = 0x08048000;

    .text : ALIGN(4K) {
        *(.text._start)
        *(.text*)
    }

    .rodata : ALIGN(4K) {
        *(.rodata*)
    }

    .data : ALIGN(4K) {
        *(.data*)
    }

    .bss : ALIGN(4K) {
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        __bss_end = .;
    }

    /* Dynamic sections — populated by linker, resolved by kernel */
    .dynamic : ALIGN(4K) {
        __dynamic_start = .;
        *(.dynamic)
    }

    .dynsym : ALIGN(4K) {
        __dynsym_start = .;
        *(.dynsym)
    }

    .dynstr : ALIGN(4K) {
        __dynstr_start = .;
        *(.dynstr)
    }

    .rel.plt : ALIGN(4K) {
        *(.rel.plt)
    }

    .got.plt : ALIGN(4K) {
        *(.got.plt)
    }

    /DISCARD/ : {
        *(.eh_frame)
        *(.comment)
        *(.note*)
    }
}
```

## Compilation Commands

### Shared Library
```bash
# Compile each source as PIC
i686-elf-gcc -fPIC -c -o stdio.o stdio.c
i686-elf-gcc -fPIC -c -o string.o string.c
...

# Link into shared object
i686-elf-gcc -fPIC -shared -nostdlib -T libc_so.ld -o libc.so *.o
```

### Dynamically-Linked App
```bash
# Compile as PIC
i686-elf-gcc -fPIC -c -o cat.o cat.c

# Link with dynamic script
# -N: omit .comment, allow undefined symbols
# The linker will create .rel.plt entries for undefined symbols
i686-elf-ld -T linker_dynamic.ld -N -o cat.bin cat.o
```

### Statically-Linked App (unchanged)
```bash
i686-elf-gcc -c -o cat.o cat.c
i686-elf-ld -T linker.ld -o cat.bin cat.o ../libc/build/libc.o
```

## Files Summary

| File | Action | Purpose |
|------|--------|---------|
| `docs/DYNAMIC_LIBS.md` | NEW | This document |
| `docs/PLAN.md` | EDIT | Update phase tracking |
| `libc/libc_so.ld` | NEW | Linker script for shared library |
| `libc/Makefile` | EDIT | Add `build/libc.so` target |
| `apps/linker_dynamic.ld` | NEW | Linker script for dynamic apps |
| `apps/Makefile` | EDIT | Add `make dynamic` rules |
| `kernel/src/include/proc/elf.h` | EDIT | Add ELF dynamic types and structs |
| `kernel/src/proc/elf.c` | EDIT | Extend `load_elf()` for dynamic linking |
| `kernel/src/include/fs/vfs.h` | EDIT | Add `shared_lib_t` struct |
| `kernel/src/fs/vfs.c` | EDIT | Add `load_shared_libs()` |
| `kernel/src/include/proc/process.h` | EDIT | Add `lib_base` to PCB |
| `kernel/src/proc/process.c` | EDIT | Initialize `lib_base` |
| `kernel/src/kmain.c` | EDIT | Call `load_shared_libs()` |
| `lib/` | NEW DIR | Directory for shared libraries |
| `Makefile` | EDIT | Add `lib/` to initrd, build libc.so |
