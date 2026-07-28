#include <unistd.h>
#include <graphics.h>

#define CURSOR_SIZE 8

static void draw_cursor(framebuffer_t *fb, int x, int y, unsigned int color)
{
    gfx_fill_rect(fb, x, y, CURSOR_SIZE, 2, color);
    gfx_fill_rect(fb, x, y + 2, 2, CURSOR_SIZE - 2, color);
}

int main(void)
{
    framebuffer_t fb;
    if (gfx_init(&fb, 320, 200, 32) < 0)
        return 1;

    gfx_font_load("/fonts/vga.psf");

    int mouse_fd = open("/mouse", "r");
    if (mouse_fd < 0) {
        gfx_text(&fb, 10, 10, "Failed to open /mouse", 0xFF0000, 0x000000);
        sleep(3);
        return 1;
    }

    int cx = 160, cy = 100;
    unsigned char buf[60];
    int draw_x = -1, draw_y = -1;

    gfx_clear(&fb, 0x000000);
    gfx_text(&fb, 10, 10, "Move the mouse!", 0x00FF00, 0x000000);
    draw_cursor(&fb, cx, cy, 0xFFFFFF);

    for (;;) {
        int n = read(mouse_fd, buf, sizeof(buf));
        if (n < 3) continue;

        /* Only process complete 3-byte packets, discard remainder */
        int pkts = n / 3;
        for (int i = 0; i < pkts; i++) {
            int dx = (int)(signed char)buf[i * 3 + 1];
            int dy = (int)(signed char)buf[i * 3 + 2];

            cx += dx;
            cy -= dy;
            if (cx < 0) cx = 0;
            if (cy < 0) cy = 0;
            if (cx > 312) cx = 312;
            if (cy > 192) cy = 192;
        }

        if (pkts > 0) {
            if (draw_x >= 0)
                draw_cursor(&fb, draw_x, draw_y, 0x000000);

            unsigned char last_buttons = buf[(pkts - 1) * 3 + 0];
            unsigned int color = (last_buttons & 0x01) ? 0xFF0000 :
                                 (last_buttons & 0x02) ? 0x0000FF : 0xFFFFFF;
            draw_cursor(&fb, cx, cy, color);
            draw_x = cx;
            draw_y = cy;
        }
    }

    return 0;
}
