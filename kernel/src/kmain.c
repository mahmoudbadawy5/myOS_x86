#include <types.h>
#include <arch.h>
#include <string.h>
#include <vga.h>
#include <idt.h>
#include <gdt.h>
#include <isr.h>
#include <irq.h>
#include <timer.h>
#include <kbd.h>
#include <stdio.h>
#include <multiboot.h>
#include <mem/phys_mem.h>
#include <mem/virt_mem.h>
#include <mem/malloc.h>
#include <fs/initrd.h>
#include <math.h>
#include <arch/syscalls.h>
#include <process.h>

extern unsigned int code, end;
unsigned int kstart = (unsigned int)&code - KERNEL_VIRTUAL_BASE;
unsigned int kend = (unsigned int)&end - KERNEL_VIRTUAL_BASE;
uint32_t initrd_location, initrd_end;

void print_mmap(multiboot_info_t *mbd)
{
    puts("Memory Map: \n");
    for (int i = 0; i < mbd->mmap_length; i += sizeof(multiboot_memory_map_t))
    {
        multiboot_memory_map_t *mmmt =
            (multiboot_memory_map_t *)(mbd->mmap_addr + KERNEL_VIRTUAL_BASE + i);
        printf("Start Addr: 0x%08ux%08ux | Length: 0x%08ux%08ux | Type: %s\n",
               mmmt->addr_high, mmmt->addr_low, mmmt->len_high, mmmt->len_low, (mmmt->type == 1 ? "Available" : "Reserved"));
    }
}

void print_hex(uint32_t x)
{
    puts("0x");
    for (int i = 7; i >= 0; i--)
    {
        int mask = (0xF << (4 * i));
        int dig = (x & mask) >> (4 * i);
        if (dig <= 9)
            putch(dig + '0');
        else
            putch(dig - 10 + 'A');
    }
    putch('\n');
}

void init_memory_regions(unsigned long magic, multiboot_info_t *mbd)
{
    puts("KSTART: ");
    print_hex(kstart);
    initrd_location = *((uint32_t *)(mbd->mods_addr + KERNEL_VIRTUAL_BASE));
    initrd_end = *(uint32_t *)(mbd->mods_addr + KERNEL_VIRTUAL_BASE + 4);

    puts("KEND: ");
    print_hex(kend);
    puts("initrd_end: ");
    print_hex(initrd_end);
    init_phys_mem(max(kend, initrd_end));
    puts("init done :D\n");
    for (int i = 0; i < mbd->mmap_length; i += sizeof(multiboot_memory_map_t))
    {
        multiboot_memory_map_t *mmmt =
            (multiboot_memory_map_t *)(mbd->mmap_addr + KERNEL_VIRTUAL_BASE + i);
        if (mmmt->type == MULTIBOOT_MEMORY_AVAILABLE)
        {
            initialize_memory_region(mmmt->addr_low, mmmt->len_low);
        }
    }
    if (magic != MULTIBOOT_BOOTLOADER_MAGIC)
    {
        // Panic is not available
        puts("\x1b\x40");
        puts("Invalid magic number!!");
        __asm__("cli; hlt;");
    }

    if (!(mbd->flags >> 6 & 0x1))
    {
        puts("\x1b\x40");
        puts("invalid memory map given by GRUB bootloader");
        __asm__("cli; hlt;");
    }

    deinitialize_memory_region(0, kend + MAX_BLOCK_ENTRIES * 4); // reserve first 1M for grub/bios + Reserve kernel space + (old page tables)
}

void app1_main() {
    fs_node_t *program = get_node("/test1.bin", root_dir);
    if (!program) {
        printf("Failed to read test1.bin\n");
        return;
    }
    void *program_phys_page = alloc_blocks(1);
    map_address((void*)0x40000000, program_phys_page);
    void *program_content = (void *)0x40000000;
    read_fs(program, program->size, 1, (uint8_t *)program_content);
    void (*entry_point)() = (void (*)())program_content;
    entry_point();
}

void app2_main() {
    fs_node_t *program = get_node("/test2.bin", root_dir);
    if (!program) {
        printf("Failed to read test2.bin\n");
        return;
    }
    void *program_phys_page = alloc_blocks(1);
    map_address((void*)0x40000000, program_phys_page);
    void *program_content = (void *)0x40000000;
    read_fs(program, program->size, 1, (uint8_t *)program_content);
    void (*entry_point)() = (void (*)())program_content;
    entry_point();
}

void kmain(unsigned long magic, multiboot_info_t *mbd)
{
    init_video();
    init_memory_regions(magic, mbd);
    init_paging();
    init_malloc();
    files_open = malloc(sizeof(FILE *) * MAX_FILES);
    init_stdfiles();
    puts("Hello World!");
    printf("Initializing video:\t\t[\x1b\x02OK\x1b\x0F]\n");
    printf("Initializing memory:\t[\x1b\x02OK\x1b\x0F]\n");

    printf("Initializing Paging:\t[\x1b\x02OK\x1b\x0F]\n");

    printf("Initializing malloc:\t[\x1b\x02OK\x1b\x0F]\n");

    printf("Initializing GDT:\t\t[");
    gdt_install();
    printf("\x1b\x02OK\x1b\x0F]\n");

    printf("Initializing IDT:\t\t[");
    idt_install();
    printf("\x1b\x02OK\x1b\x0F]\n");

    printf("Initializing ISR:\t\t[");
    isrs_install();
    printf("\x1b\x02OK\x1b\x0F]\n");

    printf("Initializing IRQ:\t\t[");
    irq_install();
    printf("\x1b\x02OK\x1b\x0F]\n");

    printf("Initializing Timer:\t\t[");
    timer_install();
    printf("\x1b\x02OK\x1b\x0F]\n");

    printf("Initializing Keyboard:\t[");
    keyboard_install();
    printf("\x1b\x02OK\x1b\x0F]\t\n");

    printf("Initializing Syscalls:\t[");
    init_syscalls();
    printf("\x1b\x02OK\x1b\x0F]\t\n");

    init_initrd(initrd_location + KERNEL_VIRTUAL_BASE);
    init_multitasking();

    create_process("test1");
    create_process(app2_main);

    printf("Kernel loaded at %08ux, ends at: %08ux\n", kstart, kend);

    print_mmap(mbd);

    __asm__ __volatile__("sti");

    for (;;) 
        ;
}
