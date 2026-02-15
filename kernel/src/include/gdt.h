#ifndef _GDT_H
#define _GDT_H

#include <types.h>

/* Defines a GDT entry. We say packed, because it prevents the
*  compiler from doing things that it thinks is best: Prevent
*  compiler "optimization" by packing */
struct gdt_entry
{
    unsigned short limit_low;
    unsigned short base_low;
    unsigned char base_middle;
    unsigned char access;
    unsigned char granularity;
    unsigned char base_high;
} __attribute__((packed));

/* Special pointer which includes the limit: The max bytes
*  taken up by the GDT, minus 1. Again, this NEEDS to be packed */
struct gdt_ptr
{
    unsigned short limit;
    unsigned int base;
} __attribute__((packed));

/* Our GDT: null, kernel code, kernel data, user code (0x1B), user data (0x23), TSS (0x28) */
struct gdt_entry gdt[6];
struct gdt_ptr gp;

void gdt_set_tss(int num, uint32_t base, uint32_t limit);

/* This will be a function in start.asm. We use this to properly
*  reload the new segment registers */
extern void gdt_flush();

extern void gdt_install();


#endif