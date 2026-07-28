#ifndef _VGA_H
#define _VGA_H

#include <types.h>
#include <fs/vfs.h>

void init_video(void);
void init_stdfiles(void);
void scroll(void);
void move_csr(void);
void cls(void);
void putch(char c);
void puts(char *str);
void settextcolor(unsigned char forecolor, unsigned char backcolor);

/* Restore VGA text mode (mode 3) after being in a VBE graphics mode.
 * Reprograms VGA registers via I/O ports. Text memory at 0xB8000 is
 * untouched during graphics mode, so old content reappears. */
void vga_restore_text_mode(void);

extern fs_node_t *stdout_node;

#endif
