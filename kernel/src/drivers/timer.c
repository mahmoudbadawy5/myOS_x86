#include <irq.h>
#include <isr.h>
#include <vga.h>

int ticks = 0;

void timer_handler(struct regs *r)
{
    ticks++;
    /*if (ticks % 18 == 0)
    {
        puts("One second has passed\n");
    }*/
}

void timer_install()
{
    irq_install_handler(0, timer_handler);
}