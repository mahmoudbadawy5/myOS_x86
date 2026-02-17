#include <tss.h>
#include <gdt.h>
#include <arch.h>

static tss_entry_t tss;
static uint8_t exception_stack[TSS_EXCEPTION_STACK_SIZE];

extern void tss_flush(uint16_t sel);

void tss_install(void)
{
    uint32_t base = (uint32_t)&tss;
    uint32_t limit = sizeof(tss_entry_t) - 1;

    tss.prev_tss = 0;
    tss.esp0 = (uint32_t)exception_stack + TSS_EXCEPTION_STACK_SIZE;
    tss.ss0 = 0x10;
    tss.esp1 = 0;
    tss.ss1 = 0;
    tss.esp2 = 0;
    tss.ss2 = 0;
    tss.cr3 = 0;
    tss.eip = 0;
    tss.eflags = 0;
    tss.eax = 0;
    tss.ecx = 0;
    tss.edx = 0;
    tss.ebx = 0;
    tss.esp = 0;
    tss.ebp = 0;
    tss.esi = 0;
    tss.edi = 0;
    tss.es = 0;
    tss.cs = 0;
    tss.ss = 0;
    tss.ds = 0;
    tss.fs = 0;
    tss.gs = 0;
    tss.ldt = 0;
    tss.trap = 0;
    tss.iomap_base = sizeof(tss_entry_t);

    gdt_set_tss(5, base, limit);
    tss_flush(0x28);
}

void tss_set_stack(uint32_t kernel_ss, uint32_t kernel_esp)
{
    tss.ss0 = kernel_ss;
    tss.esp0 = kernel_esp;
}
