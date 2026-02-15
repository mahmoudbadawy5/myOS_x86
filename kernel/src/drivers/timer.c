#include <irq.h>
#include <isr.h>
#include <vga.h>
#include <process.h>

int ticks = 0;

void timer_handler(struct regs *r)
{
    ticks++;
    schedule();
}

void timer_install()
{
    irq_install_handler(0, timer_handler);
}