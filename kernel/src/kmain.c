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
#include <tss.h>

extern unsigned int code, end;
unsigned int kstart = (unsigned int)&code - KERNEL_VIRTUAL_BASE;
unsigned int kend = (unsigned int)&end - KERNEL_VIRTUAL_BASE;
uint32_t initrd_location, initrd_end;

void print_mmap(multiboot_info_t *mbd)
{
    puts("Memory Map: \n");
    for (int i = 0; i < mbd->mmap_length;
         i += sizeof(multiboot_memory_map_t))
    {
        multiboot_memory_map_t *mmmt =
            (multiboot_memory_map_t *)(mbd->mmap_addr + KERNEL_VIRTUAL_BASE + i);
        printf("Start Addr: 0x%08ux%08ux | Length: 0x%08ux%08ux | Type: %s\n",
               mmmt->addr_high, mmmt->addr_low, mmmt->len_high, mmmt->len_low, (mmmt->type == 1 ? "Available" : "Reserved"));
    }
}

void virt_mem_test()
{
    // // Let's do a page fault :)

    // Let's map an address

    map_address((void *)0xC0000000, (void *)0x00200000);
    printf("DONE\n");

    uint32_t *test_fault = (uint32_t *)0xC0000000;
    test_fault[0] = 15;
    int x = test_fault[0];
    printf("Read: %d\n", x);

    uint32_t *test = alloc_blocks(2);
    uint32_t *test2 = alloc_blocks(3);
    uint32_t *test3 = alloc_blocks(1);
    printf("Allocated memory at %08ux\n", (uint32_t)test);
    printf("Allocated memory at %08ux\n", (uint32_t)test2);
    printf("Allocated memory at %08ux\n", (uint32_t)test3);
    free_blocks(test, 2);
    free_blocks(test2, 3);
    free_blocks(test3, 1);
}

void malloc_test()
{
    // Malloc test

    uint32_t *arr = (uint32_t *)malloc(5 * sizeof(uint32_t));
    arr[0] = 10;
    arr[1] = 15;
    arr[2] = 16;
    arr[3] = 11;
    arr[4] = 2;
    for (int i = 0; i < 5; i++)
        printf("arr[%d] = %d\n", i, arr[i]);
    printf("Address of arr: %ux\n", (uint32_t)arr);
    free(arr);

    uint32_t *arr2 = (uint32_t *)malloc(6 * sizeof(uint32_t));
    arr2[0] = 10;
    arr2[1] = 15;
    // arr2[2] = 16;
    arr2[3] = 11;
    arr2[4] = 2;
    uint32_t *arr3 = (uint32_t *)malloc(10 * sizeof(uint32_t));
    for (int i = 0; i < 10; i++)
        arr3[i] = i * 10 + 5;

    for (int i = 0; i < 6; i++)
        printf("arr2[%d] = %d\n", i, arr2[i]);
    printf("Address of arr2: %ux\n", (uint32_t)arr2);

    for (int i = 0; i < 10; i++)
        printf("arr3[%d] = %d\n", i, arr3[i]);
    printf("Address of arr3: %ux\n", (uint32_t)arr3);
    free(arr2);
    free(arr3);
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
    for (int i = 0; i < mbd->mmap_length;
         i += sizeof(multiboot_memory_map_t))
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
    tss_install();
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

    printf("Initializing multitasking:\t[");
    init_multitasking();
    create_process("/echo.bin");
    printf("\x1b\x02OK\x1b\x0F]\n");

    printf("Kernel loaded at %08ux, ends at: %08ux\n", kstart, kend);

    print_mmap(mbd);

    __asm__ __volatile__("sti");

    char *file_content = malloc(1024);
    fs_node_t *file = get_node("/test_folder/test.txt", root_dir);
    if (!file)
    {
        printf("Failed to read\n");
    }
    else if (!file->read)
    {
        printf("Read function is not ready?");
    }
    else
    {
        read_fs(file, file->size, 1, (uint8_t *)file_content);
        file_content[file->size] = '\0';
        printf("%s\n", file_content);
    }

    printf("\n\n\n");

    fs_node_t *file2 = get_node("///test_folder//depth2_folder//depth3_file.txt", root_dir);
    if (!file2)
    {
        printf("Failed to read\n");
    }
    else if (!file2->read)
    {
        printf("Read function is not ready?");
    }
    else
    {
        read_fs(file2, file2->size, 1, (uint8_t *)file_content);
        file_content[file2->size] = '\0';
        printf("%s\n", file_content);
    }

    for (;;)
        ;
}