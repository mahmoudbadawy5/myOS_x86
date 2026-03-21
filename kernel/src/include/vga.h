#pragma once

#include <fs/vfs.h>

extern void cls();
extern void putch(char c);
extern void puts(char *str);
extern void settextcolor(unsigned char forecolor, unsigned char backcolor);
extern void init_video();
void init_stdfiles();