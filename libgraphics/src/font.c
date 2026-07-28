#include <graphics.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    unsigned char *data;
    int width;
    int height;
    int num_glyphs;
    int bytes_per_glyph;
} font_t;

static font_t current_font;
static int font_loaded = 0;

/* PSF1 header: byte0=0x36 magic, byte1=mode, byte2=height */
int gfx_font_load(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return -1;

    /* Read PSF1 header (3 bytes) */
    unsigned char header[3];
    if (fread(header, 1, 3, f) != 3) {
        fclose(f);
        return -1;
    }

    if (header[0] != 0x36) {
        fclose(f);
        return -1;
    }

    int height = header[2];
    if (height <= 0 || height > 32) {
        fclose(f);
        return -1;
    }

    int num_glyphs = 256;
    int bytes_per_glyph = height;
    int data_size = num_glyphs * bytes_per_glyph;

    unsigned char *data = (unsigned char *)malloc(data_size);
    if (!data) {
        fclose(f);
        return -1;
    }

    if (fread(data, 1, data_size, f) != (unsigned int)data_size) {
        free(data);
        fclose(f);
        return -1;
    }
    fclose(f);

    /* Free previous font if any */
    if (font_loaded && current_font.data)
        free(current_font.data);

    current_font.data = data;
    current_font.width = 8;
    current_font.height = height;
    current_font.num_glyphs = num_glyphs;
    current_font.bytes_per_glyph = bytes_per_glyph;
    font_loaded = 1;

    return 0;
}

void gfx_char(framebuffer_t *fb, int x, int y, char c, unsigned int fg, unsigned int bg)
{
    if (!font_loaded)
        return;

    int idx = (unsigned char)c;
    const unsigned char *glyph = &current_font.data[idx * current_font.bytes_per_glyph];

    for (int row = 0; row < current_font.height; row++) {
        unsigned char bits = glyph[row];
        for (int col = 0; col < current_font.width; col++) {
            unsigned int color = (bits & (0x80 >> col)) ? fg : bg;
            gfx_put_pixel(fb, x + col, y + row, color);
        }
    }
}

void gfx_text(framebuffer_t *fb, int x, int y, const char *str, unsigned int fg, unsigned int bg)
{
    while (*str) {
        gfx_char(fb, x, y, *str, fg, bg);
        x += current_font.width;
        str++;
    }
}

void gfx_text_len(framebuffer_t *fb, int x, int y, const char *str, int len, unsigned int fg, unsigned int bg)
{
    for (int i = 0; i < len && str[i]; i++) {
        gfx_char(fb, x, y, str[i], fg, bg);
        x += current_font.width;
    }
}
