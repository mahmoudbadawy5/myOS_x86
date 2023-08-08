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

extern unsigned int code, end;
unsigned int kstart = (unsigned int)&code;
unsigned int kend = (unsigned int)&end;

void kmain(unsigned long magic, multiboot_info_t *mbd)
{
    init_video();
    printf("Initializing video:\t\t[\x1b\x02OK\x1b\x0F]\n");
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

    __asm__ __volatile__("sti");

    if (magic != MULTIBOOT_BOOTLOADER_MAGIC)
    {
        panic("Invalid magic number!!");
    }

    if (!(mbd->flags >> 6 & 0x1))
    {
        panic("invalid memory map given by GRUB bootloader");
    }

    puts("Memory Map: \n");

    /* Loop through the memory map and display the values */
    int i;
    for (i = 0; i < mbd->mmap_length;
         i += sizeof(multiboot_memory_map_t))
    {
        multiboot_memory_map_t *mmmt =
            (multiboot_memory_map_t *)(mbd->mmap_addr + i);

        /*puts("Ftart Addr: 0x");
        print_uint(mmmt->addr_high, 16, 8);
        print_uint(mmmt->addr_low, 16, 8);
        puts(" | Length: 0x");
        print_uint(mmmt->len_high, 16, 8);
        print_uint(mmmt->len_low, 16, 8);
        puts(" | Type: ");
        print_uint(mmmt->type, 10, 1);
        puts("\n");*/

        printf("Start Addr: 0x%08ux%08ux | Length: 0x%08ux%08ux | Type: %s\n",
               mmmt->addr_high, mmmt->addr_low, mmmt->len_high, mmmt->len_low, (mmmt->type == 1 ? "Available" : "Reserved"));
        // puts(mem_types[mmmt->type > 0]);

        if (mmmt->type == MULTIBOOT_MEMORY_AVAILABLE)
        {
            /*
             * Do something with this memory block!
             * BE WARNED that some of memory shown as availiable is actually
             * actively being used by the kernel! You'll need to take that
             * into account before writing to memory!
             */
        }
    }
    puts("Hello World!\n");

    for (;;)
        ;
}