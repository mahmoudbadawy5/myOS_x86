# Phase 7: VBE Framebuffer Graphics

## Goal
Graphics apps callable from terminal. Ctrl+C kills them and restores the old terminal text automatically. VGA text memory at `0xB8000` is never touched during graphics mode.

## Architecture

### Memory Layout

```
0x00000000 ┌──────────────────────┐
           │ Low memory (< 1MB)   │
           │ 0x8000: RM stub      │ ← Real-mode callback trampoline
           │ 0x8100: RM params    │ ← BIOS call parameter block
0x000B8000 ├──────────────────────┤
           │ VGA TEXT BUFFER (4KB) │ ← Untouched during graphics mode
0x000BC000 ├──────────────────────┤
           │ ...                  │
0xFD000000 ├──────────────────────┤
           │ VBE FRAMEBUFFER      │ ← Separate physical region
           │ (640×480×4 = 1.2MB) │
0xFD12C000 └──────────────────────┘
```

**Key insight**: VGA text memory (`0xB8000`) and VBE framebuffer (`0xFD000000`) are completely separate physical regions. The text buffer is never modified during graphics mode, so restoring VGA text mode makes old terminal text reappear automatically.

### Boot Flow
1. GRUB boots in VGA text mode (no `gfxpayload=keep`)
2. Kernel init, shell run normally in text mode
3. Graphics app calls `fb_set_mode(640, 480, 32)` → BIOS switches to VBE
4. App calls `fb_map()` → framebuffer mapped into user space
5. App draws to framebuffer
6. Ctrl+C → SIGINT → app killed → `exit()` → auto-restore VGA text mode
7. VGA text memory reappears → old terminal text intact

### Mode Switching
- **Text → VBE**: BIOS `int 0x10` (AX=0x4F02) via real-mode callback trampoline in low memory
- **VBE → Text**: VGA register reprogramming via I/O ports (no BIOS needed)

## Implementation

### Step 1: Identity Mapping Fix
**File**: `kernel/src/mem/virt_mem.c`

The real-mode callback stub resides at physical `0x8000` below 1MB. After `init_paging()`, there is no identity mapping — only the higher-half mapping at entry 768.

**Fix in `init_paging()`** (~line 131):
```c
kernel_page_dir[0] = kernel_page_dir[KERNEL_PAGE_NUMBER];
```

This shares the same page table for both identity (entry 0) and higher-half (entry 768) mappings of the first 4MB. Process page directories are NOT changed — `vmm_clone_directory()` is untouched.

**During BIOS calls**: The kernel calls `switch_to_kernel_page_dir()` before the real-mode stub, then restores the process's page directory after.

### Step 2: Real-Mode Callback Trampoline

**New file**: `kernel/src/arch/real_mode_stub.asm`

A small ASM stub at physical `0x8000` (below 1MB, identity-mapped in kernel page dir). Parameter block at `0x8100`.

**Flow**:
1. C caller writes BIOS call params to `0x8100` (AX, BX values)
2. C caller calls `switch_to_kernel_page_dir()`
3. C caller saves protected-mode state (GDT, IDT, CR3, CR0)
4. C caller loads real-mode-compatible segment registers, clears `CR0.PE`
5. C caller far-jumps to `0x8000`
6. Stub (real mode): sets up segments, calls `int 0x10`
7. Stub: writes return value to `0x8104`
8. Stub: sets `CR0.PE`, reloads GDT, far-jumps back to protected mode
9. C caller restores process page directory, returns

**Parameter block** (`0x8100`):

| Offset | Size | Content |
|--------|------|---------|
| 0x8100 | 2 | AX (VBE function, e.g., `0x4F02`) |
| 0x8102 | 2 | BX (mode number, e.g., `0x4112`) |
| 0x8104 | 2 | Return AX |
| 0x8106 | 2 | Return status (0 = success) |

**New file**: `kernel/src/include/arch/real_mode.h`
```c
int vbe_set_mode(uint16_t width, uint16_t height, uint16_t bpp);
```

**File**: `kernel/src/kmain.c` — copy stub to `0x8000` during early init (before paging, using identity-mapped memcpy).

### Step 3: VGA Text Mode Restore

**File**: `kernel/src/drivers/vga.c` — add `vga_restore_text_mode()`

Reprogram VGA controller to mode 3 via I/O ports (~40 register writes):
- Sequencer: ports `0x3C4`/`0x3C5`
- CRTC: ports `0x3D4`/`0x3D5`
- Graphics controller: ports `0x3CE`/`0x3CF`
- Attribute controller: ports `0x3C0`/`0x3C1`

Use standard VGA text mode register values (well-documented). After restore, text memory at `0xB8000` reappears automatically.

**New file**: `kernel/src/include/drivers/vga.h` — declare `vga_restore_text_mode()`

### Step 4: New Syscalls (#33, #34, #35)

| # | Name | Handler | Args | Description |
|---|------|---------|------|-------------|
| 33 | `fb_set_mode` | `syscall_fb_set_mode` | EBX=width, ECX=height, EDX=bpp | Save VGA state, call `vbe_set_mode()` via real-mode callback. Store framebuffer phys addr + pitch in kernel globals. |
| 34 | `fb_map` | `syscall_fb_map` | — | Map framebuffer phys addr into user space at `0xFD000000` via `map_address_user()`. Set `current_process->has_framebuffer = 1`. Return virtual addr. |
| 35 | `fb_restore_text` | `syscall_fb_restore_text` | — | Call `vga_restore_text_mode()`. Set `has_framebuffer = 0`. |

**Auto-restore on exit**: In `syscall_exit()`, before cleanup:
```c
if (current_process->has_framebuffer)
    vga_restore_text_mode();
```

**Files**:

| File | Change |
|------|--------|
| `kernel/src/arch/syscalls.c` | Add 3 handlers, update `syscall_exit()` |
| `kernel/src/arch/syscalls_asm.asm` | `MAX_SYSCALL EQU 32` → `MAX_SYSCALL EQU 35` |
| `kernel/src/include/arch/syscalls.h` | `MAX_SYSCALLS` → 36, add 3 declarations |
| `kernel/src/proc/process.h` | Add `int has_framebuffer;` to PCB |
| `kernel/src/proc/process.c` | `init_process()` sets `has_framebuffer = 0` |

### Step 5: libc Stubs + libgraphics

**File**: `libc/src/syscalls.c` — raw stubs:
```c
int sys_fb_set_mode(int w, int h, int bpp);
void *sys_fb_map(void);
int sys_fb_restore_text(void);
```

**File**: `libc/src/include/unistd.h` — POSIX wrappers:
```c
int fb_set_mode(int w, int h, int bpp);
void *fb_map(void);
int fb_restore_text(void);
```

**New directory**: `libgraphics/` (modeled after `libmath/`)

| File | Purpose |
|------|---------|
| `libgraphics/src/graphics.c` | Drawing primitives |
| `libgraphics/src/include/graphics.h` | Header |
| `libgraphics/Makefile` | Builds `libgraphics.so` |

**API**:
```c
void gfx_init(void);           // calls fb_map(), stores framebuffer ptr
void gfx_shutdown(void);       // calls fb_restore_text()
void gfx_putpixel(int x, int y, uint32_t color);
void gfx_clear(uint32_t color);
void gfx_rect(int x, int y, int w, int h, uint32_t color);
void gfx_circle(int cx, int cy, int r, uint32_t color);
void gfx_line(int x1, int y1, int x2, int y2, uint32_t color);
```

**File**: `Makefile` — add `libgraphics` to build, initrd copy, clean targets
**File**: `apps/Makefile` — link graphics apps against `libgraphics.so`

### Step 6: Test Applications

| File | Description |
|------|-------------|
| `apps/test_gfx_colors.c` | Color gradient filling the screen |
| `apps/test_gfx_shapes.c` | Rectangles, circles, lines |
| `apps/test_gfx_interactive.c` | Keyboard-driven cursor drawing |

All call `gfx_init()` at start, `gfx_shutdown()` at end. Ctrl+C → SIGINT → `exit()` → auto-restore via `has_framebuffer` check.

### Step 7: Build & Config

**File**: `isodir/boot/grub/grub.cfg` — no `gfxpayload=keep` (text mode boot, VBE switch at runtime)

### Execution Order

1. Identity mapping fix (`virt_mem.c`)
2. Real-mode stub + VGA restore (`real_mode_stub.asm`, `vga.c`, headers)
3. Syscalls 33-35 + PCB `has_framebuffer` field
4. libc stubs
5. libgraphics + Makefile updates
6. Test apps
7. Build, test, commit

### New Files Summary

| File | Purpose |
|------|---------|
| `kernel/src/arch/real_mode_stub.asm` | Real-mode callback trampoline |
| `kernel/src/include/arch/real_mode.h` | Header |
| `kernel/src/include/drivers/vga.h` | VGA restore header |
| `libgraphics/src/graphics.c` | Drawing primitives |
| `libgraphics/src/include/graphics.h` | libgraphics header |
| `libgraphics/Makefile` | Build libgraphics.so |
| `apps/test_gfx_colors.c` | Color gradient test |
| `apps/test_gfx_shapes.c` | Shapes test |
| `apps/test_gfx_interactive.c` | Interactive drawing test |
