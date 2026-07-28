#include <graphics.h>
#include <unistd.h>

int gfx_init(framebuffer_t *fb, int width, int height, int bpp)
{
    print("gfx_init: entered\n");
    int ret = fb_set_mode(width, height, bpp);
    print("gfx_init: fb_set_mode returned\n");
    if (ret < 0)
        return -1;

    void *addr = fb_map();
    if (!addr)
        return -1;

    fb->addr = addr;
    fb->width = width;
    fb->height = height;
    fb->bpp = bpp;
    fb->pitch = width * (bpp / 8);
    return 0;
}

void gfx_put_pixel(framebuffer_t *fb, int x, int y, unsigned int color)
{
    if (x < 0 || x >= fb->width || y < 0 || y >= fb->height)
        return;
    unsigned int off = y * fb->pitch + x * (fb->bpp / 8);
    if (fb->bpp == 32) {
        unsigned int *p = (unsigned int *)((unsigned char *)fb->addr + off);
        *p = color;
    } else if (fb->bpp == 24) {
        unsigned char *p = (unsigned char *)fb->addr + off;
        p[0] = color & 0xFF;
        p[1] = (color >> 8) & 0xFF;
        p[2] = (color >> 16) & 0xFF;
    } else if (fb->bpp == 16) {
        unsigned short *p = (unsigned short *)((unsigned char *)fb->addr + off);
        *p = (unsigned short)color;
    }
}

void gfx_clear(framebuffer_t *fb, unsigned int color)
{
    for (int y = 0; y < fb->height; y++)
        for (int x = 0; x < fb->width; x++)
            gfx_put_pixel(fb, x, y, color);
}

void gfx_hline(framebuffer_t *fb, int x0, int x1, int y, unsigned int color)
{
    for (int x = x0; x <= x1; x++)
        gfx_put_pixel(fb, x, y, color);
}

void gfx_vline(framebuffer_t *fb, int x, int y0, int y1, unsigned int color)
{
    for (int y = y0; y <= y1; y++)
        gfx_put_pixel(fb, x, y, color);
}

void gfx_rect(framebuffer_t *fb, int x, int y, int w, int h, unsigned int color)
{
    gfx_hline(fb, x, x + w - 1, y, color);
    gfx_hline(fb, x, x + w - 1, y + h - 1, color);
    gfx_vline(fb, x, y, y + h - 1, color);
    gfx_vline(fb, x + w - 1, y, y + h - 1, color);
}

void gfx_fill_rect(framebuffer_t *fb, int x, int y, int w, int h, unsigned int color)
{
    for (int row = y; row < y + h; row++)
        for (int col = x; col < x + w; col++)
            gfx_put_pixel(fb, col, row, color);
}
