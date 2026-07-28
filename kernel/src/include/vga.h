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

/* Save VGA font data from plane 2 before BGA mode switch destroys it.
 * Only performs the save once (guarded by font_saved flag). */
void vga_save_font(void);

/* Save cursor position and attribute before entering graphics mode.
 * Called on every bga_set_mode() so the state is always current. */
void vga_save_cursor_state(void);

extern fs_node_t *stdout_node;

#endif
