#ifndef _ARCH_H
#define _ARCH_H

#define KERNEL_VIRTUAL_BASE 0xC0000000
#define KERNEL_PAGE_NUMBER (KERNEL_VIRTUAL_BASE >> 22)

extern unsigned char inportb(unsigned short _port);
extern void outportb(unsigned short _port, unsigned char _data);

#endif