# Project Overview

This is a hobby x86 operating system written in C and Assembly. The project is built with a custom toolchain based on GCC and NASM. The OS boots using GRUB and the Multiboot standard.

The kernel features a virtual filesystem with an initial ramdisk, physical and virtual memory management, preemptive multitasking with process creation, and memory isolation for user-space applications. It also includes drivers for a keyboard, a timer, and VGA text mode.

The project is structured as follows:

*   `kernel/`: The kernel source code, including core OS components, memory management, and process management.
*   `libc/`: A custom C library.
*   `apps/`: User-space applications, which are now loaded into isolated memory spaces.
*   `tools/`: Helper scripts for building the kernel and creating the initrd.

# Building and Running

## Building

To build the entire project, run the following command from the root directory:

```bash
make all
```

This will build the kernel and the user-space applications, and create a bootable ISO image named `myos.iso`.

## Running

To run the OS in QEMU/KVM, use the following command:

```bash
make run
```

To run the OS in Bochs, use the following command:

```bash
make bochs
```

# Development Conventions

The codebase is written in C and Assembly. The C code follows the K&R style. The assembly code is written in NASM syntax.

The project uses a custom build system based on Make. The main `Makefile` in the root directory orchestrates the build process. Each component (kernel, libc, apps) has its own `Makefile`.

Recent additions include:
*   **Preemptive Multitasking**: Implemented with a round-robin scheduler and context switching via timer interrupts.
*   **Process Management**: Includes `pcb_t` structure for process control blocks and functions for `create_process`.
*   **Memory Isolation**: Each process now has its own page directory, ensuring separate virtual address spaces. User-space applications are loaded at `0x40000000`.