#ifndef _VGA_H
#define _VGA_H

extern void cls();
extern void putch(char c);
extern void puts(char *str);
extern void settextcolor(unsigned char forecolor, unsigned char backcolor);
extern void init_video();

#endif