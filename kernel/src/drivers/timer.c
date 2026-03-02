#include <arch.h>
#include <irq.h>
#include <isr.h>
#include <vga.h>
#include <process.h>
#include <stdio.h>

int ticks = 0;

void timer_handler(struct regs *r)
{
    ticks++;
    outportb(0x20, 0x20);
    schedule(r);
}

void timer_install()
{
    irq_install_handler(0, timer_handler);
}