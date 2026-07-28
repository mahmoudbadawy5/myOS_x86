#ifndef _GRAPHICS_H
#define _GRAPHICS_H

typedef struct {
    void *addr;
    int width;
    int height;
    int pitch;
    int bpp;
} framebuffer_t;

int gfx_init(framebuffer_t *fb, int width, int height, int bpp);
void gfx_put_pixel(framebuffer_t *fb, int x, int y, unsigned int color);
void gfx_clear(framebuffer_t *fb, unsigned int color);
void gfx_hline(framebuffer_t *fb, int x0, int x1, int y, unsigned int color);
void gfx_vline(framebuffer_t *fb, int x, int y0, int y1, unsigned int color);
void gfx_rect(framebuffer_t *fb, int x, int y, int w, int h, unsigned int color);
void gfx_fill_rect(framebuffer_t *fb, int x, int y, int w, int h, unsigned int color);

#endif
