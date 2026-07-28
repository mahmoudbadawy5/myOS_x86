#include <unistd.h>
#include <graphics.h>

int main(void)
{
    framebuffer_t fb;
    if (gfx_init(&fb, 320, 200, 32) < 0) {
        return 1;
    }

    gfx_clear(&fb, 0x000000);

    /* Load PSF1 font from initrd */
    if (gfx_font_load("/fonts/vga.psf") < 0) {
        gfx_fill_rect(&fb, 10, 10, 200, 16, 0xFF0000);
        /* Font load failed — show red bar as error indicator */
    } else {
        gfx_text(&fb, 10, 10, "Font loaded OK", 0x00FF00, 0x000000);
        gfx_text(&fb, 10, 30, "Hello from myOS!", 0xFFFFFF, 0x000000);
        gfx_text(&fb, 10, 50, "ABCDEFGHIJKLMNOPQRSTUVWXYZ", 0x00FFFF, 0x000000);
        gfx_text(&fb, 10, 70, "abcdefghijklmnopqrstuvwxyz", 0xFFFF00, 0x000000);
        gfx_text(&fb, 10, 90, "0123456789 !@#$%^&*()", 0xFF00FF, 0x000000);
    }

    /* Draw some shapes */
    gfx_fill_rect(&fb, 50, 120, 100, 60, 0x0000FF);
    gfx_fill_rect(&fb, 80, 110, 60, 60, 0x00FF00);
    gfx_rect(&fb, 20, 105, 280, 90, 0xFFFFFF);

    /* Wait long enough to see — Ctrl+C to kill (auto-restores text mode) */
    sleep(5);

    return 0;
}
