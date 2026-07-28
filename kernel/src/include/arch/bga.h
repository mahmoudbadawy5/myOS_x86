#ifndef _BGA_H
#define _BGA_H

#include <types.h>

/* Set a video mode via BGA (Bochs/QEMU/VirtualBox). Returns 0 on success. */
int bga_set_mode(uint16_t width, uint16_t height, uint16_t bpp);

/* Get the physical address of the linear framebuffer. */
uint32_t bga_get_lfb_addr(void);

/* Get bytes per scanline. */
uint32_t bga_get_pitch(void);

/* Get current resolution. */
uint32_t bga_get_width(void);
uint32_t bga_get_height(void);

/* Disable BGA (reverts to VGA text mode on QEMU). */
void bga_disable(void);

#endif
