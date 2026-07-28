#include <unistd.h>
#include <graphics.h>

int main(void)
{
    framebuffer_t fb;
    if (gfx_init(&fb, 320, 200, 32) < 0) {
        return 1;
    }

    gfx_clear(&fb, 0x000000);
    gfx_fill_rect(&fb, 50, 50, 100, 80, 0x0000FF);
    gfx_fill_rect(&fb, 80, 30, 60, 60, 0x00FF00);
    gfx_rect(&fb, 20, 20, 280, 160, 0xFFFFFF);

    /* Wait long enough to see — Ctrl+C to kill (auto-restores text mode) */
    for (volatile unsigned int i = 0; i < 0x7FFFFFFF; i++)
        ;

    return 0;
}
