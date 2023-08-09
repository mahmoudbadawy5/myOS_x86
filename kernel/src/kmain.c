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
#include <types.h>

extern unsigned int code, end;
unsigned int kstart = (unsigned int)&code;
unsigned int kend = (unsigned int)&end;

void print_mmap(multiboot_info_t *mbd)
{
    puts("Memory Map: \n");
    for (int i = 0; i < mbd->mmap_length;
         i += sizeof(multiboot_memory_map_t))
    {
        multiboot_memory_map_t *mmmt =
            (multiboot_memory_map_t *)(mbd->mmap_addr + i);
        printf("Start Addr: 0x%08ux%08ux | Length: 0x%08ux%08ux | Type: %s\n",
               mmmt->addr_high, mmmt->addr_low, mmmt->len_high, mmmt->len_low, (mmmt->type == 1 ? "Available" : "Reserved"));
    }
}

void kmain(unsigned long magic, multiboot_info_t *mbd)
{
    init_phys_mem(kend);
    init_video();
    printf("Initializing video:\t\t[\x1b\x02OK\x1b\x0F]\n");
    printf("Initializing memory:\t[");

    if (magic != MULTIBOOT_BOOTLOADER_MAGIC)
    {
        panic("Invalid magic number!!");
    }

    if (!(mbd->flags >> 6 & 0x1))
    {
        panic("invalid memory map given by GRUB bootloader");
    }

    for (int i = 0; i < mbd->mmap_length;
         i += sizeof(multiboot_memory_map_t))
    {
        multiboot_memory_map_t *mmmt =
            (multiboot_memory_map_t *)(mbd->mmap_addr + i);
        if (mmmt->type == MULTIBOOT_MEMORY_AVAILABLE)
        {
            initialize_memory_region(mmmt->addr_low, mmmt->len_low);
        }
    }
    deinitialize_memory_region(0, 0x00100000);                                   // reserve first 1M for grub/bios
    deinitialize_memory_region(kstart, (kend - kstart) + MAX_BLOCK_ENTRIES * 4); // Reserve kernel space

    printf("\x1b\x02OK\x1b\x0F]\n");
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

    printf("Kernel loaded at %08ux, ends at: %08ux\n", kstart, kend);

    print_mmap(mbd);

    __asm__ __volatile__("sti");

    /* Loop through the memory map and display the values */
    puts("Hello World!\n");

    uint32_t *test = alloc_blocks(2);
    uint32_t *test2 = alloc_blocks(3);
    uint32_t *test3 = alloc_blocks(1);
    printf("Allocated memory at %08ux\n", (uint32_t)test);
    printf("Allocated memory at %08ux\n", (uint32_t)test2);
    printf("Allocated memory at %08ux\n", (uint32_t)test3);
    free_blocks(test, 2);
    free_blocks(test2, 3);
    free_blocks(test3, 1);

    for (;;)
        ;
}