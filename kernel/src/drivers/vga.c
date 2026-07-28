#include <vga.h>
#include <arch.h>
#include <arch/bga.h>
#include <string.h>
#include <types.h>
#include <fs/vfs.h>
#include <mem/malloc.h>
#include <stdio.h>

uint16_t *vgamem;
volatile int attrib = 0x0F;
volatile int csr_x = 0, csr_y = 0, col_next = 0;

/* Saved VGA font data from plane 2 (256 chars * 32 bytes slot = 8192 bytes).
 * Must survive BGA mode switches which destroy VGA plane contents. */
static uint8_t vga_font_buf[8192];
int font_saved = 0;

/* Saved cursor state for restore after graphics mode */
static int saved_csr_x = 0, saved_csr_y = 0, saved_attrib = 0x0F;

/* VGA memory window at physical 0xA0000 */
#define VGA_MEM  ((volatile uint8_t *)(0xA0000 + KERNEL_VIRTUAL_BASE))

void vga_save_font(void)
{
    /* 1. Configure VGA registers to map Plane 2 to 0xA0000 */
    outportb(0x3C4, 0x00); outportb(0x3C5, 0x01); /* Synchronous reset */
    outportb(0x3C4, 0x02); outportb(0x3C5, 0x04); /* Map Mask: Plane 2 write */
    outportb(0x3C4, 0x04); outportb(0x3C5, 0x07); /* Disable Chain-4 / Sequential */
    outportb(0x3C4, 0x00); outportb(0x3C5, 0x03); /* End reset */

    outportb(0x3CE, 0x04); outportb(0x3CF, 0x02); /* Read Map Select: Plane 2 */
    outportb(0x3CE, 0x05); outportb(0x3CF, 0x00); /* Mode: Planar mode */
    outportb(0x3CE, 0x06); outportb(0x3CF, 0x04); /* Map 64KB window at 0xA0000 */

    /* 2. Read 8KB font bitmap data from Plane 2 */
    for (int i = 0; i < 8192; i++)
        vga_font_buf[i] = VGA_MEM[i];

    /* 3. Restore standard text mode register routing */
    outportb(0x3C4, 0x00); outportb(0x3C5, 0x01);
    outportb(0x3C4, 0x02); outportb(0x3C5, 0x03); /* Map Mask: Planes 0 & 1 */
    outportb(0x3C4, 0x04); outportb(0x3C5, 0x02); /* Odd/Even addressing mode */
    outportb(0x3C4, 0x00); outportb(0x3C5, 0x03);

    outportb(0x3CE, 0x04); outportb(0x3CF, 0x00);
    outportb(0x3CE, 0x05); outportb(0x3CF, 0x10); /* Odd/Even mode */
    outportb(0x3CE, 0x06); outportb(0x3CF, 0x0E); /* Map text window to 0xB8000 */

    font_saved = 1;
}

void vga_save_cursor_state(void)
{
    saved_csr_x = csr_x;
    saved_csr_y = csr_y;
    saved_attrib = attrib;
}

void vga_restore_font(void)
{
    if (!font_saved)
        return;

    /* 1. Configure VGA registers to write to Plane 2 at 0xA0000 */
    outportb(0x3C4, 0x00); outportb(0x3C5, 0x01);
    outportb(0x3C4, 0x02); outportb(0x3C5, 0x04); /* Write Plane 2 */
    outportb(0x3C4, 0x04); outportb(0x3C5, 0x07); /* Disable Chain-4 */
    outportb(0x3C4, 0x00); outportb(0x3C5, 0x03);

    outportb(0x3CE, 0x04); outportb(0x3CF, 0x02); /* Read Plane 2 */
    outportb(0x3CE, 0x05); outportb(0x3CF, 0x00);
    outportb(0x3CE, 0x06); outportb(0x3CF, 0x04); /* Map to 0xA0000 */

    /* 2. Write font buffer back into Plane 2 */
    for (int i = 0; i < 8192; i++)
        VGA_MEM[i] = vga_font_buf[i];

    /* 3. Restore standard text mode register routing */
    outportb(0x3C4, 0x00); outportb(0x3C5, 0x01);
    outportb(0x3C4, 0x02); outportb(0x3C5, 0x03);
    outportb(0x3C4, 0x04); outportb(0x3C5, 0x02);
    outportb(0x3C4, 0x00); outportb(0x3C5, 0x03);

    outportb(0x3CE, 0x04); outportb(0x3CF, 0x00);
    outportb(0x3CE, 0x05); outportb(0x3CF, 0x10);
    outportb(0x3CE, 0x06); outportb(0x3CF, 0x0E); /* Map back to 0xB8000 */
}

void scroll(void)
{
    uint16_t blank = (attrib << 8) | ' ';

    if (csr_y >= 25)
    {
        uint32_t scroll_by = csr_y - 25 + 1;
        if (scroll_by > 25)
            scroll_by = 25;
        memcpy((uint8_t *)vgamem, (uint8_t *)(vgamem + 80 * (scroll_by)), (25 - scroll_by) * 80 * 2);
        memsetw(vgamem + 80 * (25 - scroll_by), blank, 80);
        csr_y = 25 - 1;
    }
}

void move_csr(void)
{
    uint32_t temp;

    /* The equation for finding the index in a linear
     *  chunk of memory can be represented by:
     *  Index = [(y * width) + x] */
    temp = csr_y * 80 + csr_x;

    /* This sends a command to indicies 14 and 15 in the
     *  CRT Control Register of the VGA controller. These
     *  are the high and low bytes of the index that show
     *  where the hardware cursor is to be 'blinking'. To
     *  learn more, you should look up some VGA specific
     *  programming documents. A great start to graphics:
     *  http://www.brackeen.com/home/vga */
    outportb(0x3D4, 14);
    outportb(0x3D5, temp >> 8);
    outportb(0x3D4, 15);
    outportb(0x3D5, temp);
}

void cls(void)
{
    uint16_t blank = (attrib << 8) | ' ';
    for (int i = 0; i < 25; i++)
        memsetw(vgamem + i * 80, blank, 80);
    move_csr();
}

void putch(char c)
{
    if (col_next)
    {
        attrib = c;
        col_next = 0;
        return;
    }
    if (c == 0x1B)
    {
        col_next = 1;
        return;
    }
    if (c == 0x0C)
    {
        cls();
        return;
    }
    if (c == 0x08)
    {
        if (csr_x != 0)
            csr_x--;
        uint16_t *where = vgamem + (csr_y * 80 + csr_x);
        *where = (' ' | (attrib << 8)); /* Character AND attributes: color */
    }
    else if (c == '\t')
    {
        csr_x += 4 - (csr_x % 4);
    }
    else if (c == '\r')
    {
        csr_x = 0;
    }
    else if (c == '\n')
    {
        csr_x = 0;
        csr_y++;
    }
    else if (c >= ' ')
    {
        uint16_t *where = vgamem + (csr_y * 80 + csr_x);
        *where = (c | (attrib << 8)); /* Character AND attributes: color */
        csr_x++;
    }
    if (csr_x >= 80)
    {
        csr_x = 0;
        csr_y++;
    }
    scroll();
    move_csr();
}

void puts(char *str)
{
    int i = 0;
    while (str[i])
        putch(str[i++]);
}

uint32_t vga_write_fs(fs_node_t *node, uint32_t size, uint32_t units, uint8_t *buffer)
{
    uint32_t ret = 0;
    col_next = 0;
    for (int i = 0; i < units; i++)
    {
        for (int j = 0; j < size; j++)
        {
            putch(*(buffer + j));
            ret++;
        }
    }
    return ret;
}

void settextcolor(unsigned char forecolor, unsigned char backcolor)
{
    /* Top 4 bytes are the background, bottom 4 bytes
     *  are the foreground color */
    attrib = (backcolor << 4) | (forecolor & 0x0F);
}

void init_video(void)
{
    vgamem = (uint16_t *)(0xB8000 + KERNEL_VIRTUAL_BASE);
    cls();
}

void init_stdfiles()
{
    stdout_node = malloc(sizeof(fs_node_t));
    stdout_node->inode = 0;
    stdout_node->write = vga_write_fs;
    stdout_node->flags |= FS_CHARDEVICE;
}

/* ---- VGA text mode restore ----
 * Reprograms the VGA controller to mode 3 (80x25 text) via I/O ports.
 * This does NOT use BIOS — it writes register values directly.
 * Text memory at 0xB8000 is never touched during graphics mode,
 * so old terminal content reappears after restore. */

void vga_restore_text_mode(void)
{
    /* Disable BGA to return to VGA output */
    bga_disable();

    /* --- Sequencer (ports 0x3C4/0x3C5) --- */
    /* Hold sequencer in Synchronous Reset (0x01) while changing clocks */
    outportb(0x3C4, 0x00); outportb(0x3C5, 0x01);

    outportb(0x3C4, 0x01); outportb(0x3C5, 0x00); /* Clocking: 8-dot mode */
    outportb(0x3C4, 0x02); outportb(0x3C5, 0x03); /* Map mask: planes 0+1 writable */
    outportb(0x3C4, 0x03); outportb(0x3C5, 0x00); /* Char map select: font A */
    outportb(0x3C4, 0x04); outportb(0x3C5, 0x02); /* Memory mode: Odd/Even, no chain-4 */

    /* --- Miscellaneous Output (port 0x3C2) --- */
    outportb(0x3C2, 0x67);         /* VCLK=28.3MHz, display RAM enabled, color */

    /* Release Sequencer Reset */
    outportb(0x3C4, 0x00); outportb(0x3C5, 0x03);

    /* --- CRTC (ports 0x3D4/0x3D5 for color) --- */
    /* Unlock CRTC protect bit first */
    outportb(0x3D4, 0x11);
    uint8_t crtc11 = inportb(0x3D5);
    outportb(0x3D5, crtc11 & 0x7F);

    outportb(0x3D4, 0x00); outportb(0x3D5, 0x5F); /* Horizontal total */
    outportb(0x3D4, 0x01); outportb(0x3D5, 0x4F); /* Horizontal display enable end */
    outportb(0x3D4, 0x02); outportb(0x3D5, 0x50); /* Horizontal blanking start */
    outportb(0x3D4, 0x03); outportb(0x3D5, 0x82); /* Horizontal blanking end */
    outportb(0x3D4, 0x04); outportb(0x3D5, 0x55); /* Horizontal retrace start */
    outportb(0x3D4, 0x05); outportb(0x3D5, 0x81); /* Horizontal retrace end */
    outportb(0x3D4, 0x06); outportb(0x3D5, 0xBF); /* Vertical total */
    outportb(0x3D4, 0x07); outportb(0x3D5, 0x1F); /* Overflow */
    outportb(0x3D4, 0x08); outportb(0x3D5, 0x00); /* Top scan line */
    outportb(0x3D4, 0x09); outportb(0x3D5, 0x4F); /* Max scan line */
    outportb(0x3D4, 0x0A); outportb(0x3D5, 0x0D); /* Cursor start */
    outportb(0x3D4, 0x0B); outportb(0x3D5, 0x0E); /* Cursor end */
    outportb(0x3D4, 0x0C); outportb(0x3D5, 0x00); /* Start address high */
    outportb(0x3D4, 0x0D); outportb(0x3D5, 0x00); /* Start address low */
    outportb(0x3D4, 0x0E); outportb(0x3D5, 0x00); /* Cursor address high */
    outportb(0x3D4, 0x0F); outportb(0x3D5, 0x00); /* Cursor address low */
    outportb(0x3D4, 0x10); outportb(0x3D5, 0x9C); /* Vertical retrace start */
    outportb(0x3D4, 0x11); outportb(0x3D5, 0x8E); /* Vertical retrace end */
    outportb(0x3D4, 0x12); outportb(0x3D5, 0x8F); /* Vertical display end */
    outportb(0x3D4, 0x13); outportb(0x3D5, 0x28); /* Offset */
    outportb(0x3D4, 0x14); outportb(0x3D5, 0x1F); /* Underline location */
    outportb(0x3D4, 0x15); outportb(0x3D5, 0x96); /* Vertical blanking start */
    outportb(0x3D4, 0x16); outportb(0x3D5, 0xB9); /* Vertical blanking end */
    outportb(0x3D4, 0x17); outportb(0x3D5, 0xA3); /* Mode control */
    outportb(0x3D4, 0x18); outportb(0x3D5, 0xFF); /* Line compare */

    /* --- Graphics Controller (ports 0x3CE/0x3CF) --- */
    outportb(0x3CE, 0x00); outportb(0x3CF, 0x00); /* Set/Reset */
    outportb(0x3CE, 0x01); outportb(0x3CF, 0x00); /* Enable Set/Reset */
    outportb(0x3CE, 0x02); outportb(0x3CF, 0x00); /* Color Compare */
    outportb(0x3CE, 0x03); outportb(0x3CF, 0x00); /* Data Rotate */
    outportb(0x3CE, 0x04); outportb(0x3CF, 0x00); /* Read Map Select */
    outportb(0x3CE, 0x05); outportb(0x3CF, 0x10); /* Mode: text mode */
    outportb(0x3CE, 0x06); outportb(0x3CF, 0x0E); /* Misc: text mode, 0xB8000 */
    outportb(0x3CE, 0x07); outportb(0x3CF, 0x0F); /* Color Don't Care */
    outportb(0x3CE, 0x08); outportb(0x3CF, 0xFF); /* Bit Mask */

    /* --- Attribute Controller (ports 0x3C0/0x3C1) --- */
    inportb(0x3DA);                 /* Reset AC flip-flop */
    for (int i = 0; i < 16; i++) {
        outportb(0x3C0, i);
        outportb(0x3C0, i);         /* Palette: identity map */
    }
    outportb(0x3C0, 0x10); outportb(0x3C0, 0x0C); /* Mode: alphanumeric + line graphics */
    outportb(0x3C0, 0x11); outportb(0x3C0, 0x00); /* Overscan */
    outportb(0x3C0, 0x12); outportb(0x3C0, 0x0F); /* Color plane enable */
    outportb(0x3C0, 0x13); outportb(0x3C0, 0x08); /* Horizontal pixel panning */
    /* Enable display */
    inportb(0x3DA);
    outportb(0x3C0, 0x20);

    /* Restore standard 16-color VGA DAC palette (RGB 6-bit, 0-63) */
    static const uint8_t vga_dac_palette[16 * 3] = {
        0x00, 0x00, 0x00,  /* 0: Black */
        0x00, 0x00, 0x2A,  /* 1: Blue */
        0x00, 0x2A, 0x00,  /* 2: Green */
        0x00, 0x2A, 0x2A,  /* 3: Cyan */
        0x2A, 0x00, 0x00,  /* 4: Red */
        0x2A, 0x00, 0x2A,  /* 5: Magenta */
        0x2A, 0x15, 0x00,  /* 6: Brown */
        0x2A, 0x2A, 0x2A,  /* 7: Light Gray */
        0x15, 0x15, 0x15,  /* 8: Dark Gray */
        0x15, 0x15, 0x3F,  /* 9: Light Blue */
        0x15, 0x3F, 0x15,  /* 10: Light Green */
        0x15, 0x3F, 0x3F,  /* 11: Light Cyan */
        0x3F, 0x15, 0x15,  /* 12: Light Red */
        0x3F, 0x15, 0x3F,  /* 13: Light Magenta */
        0x3F, 0x3F, 0x15,  /* 14: Yellow */
        0x3F, 0x3F, 0x3F   /* 15: White */
    };
    outportb(0x3C8, 0);
    for (int i = 0; i < 16 * 3; i++)
        outportb(0x3C9, vga_dac_palette[i]);

    /* Restore VGA font to plane 2 (saved before BGA mode set) */
    vga_restore_font();

    /* Reinitialize VGA state used by our driver */
    csr_x = saved_csr_x;
    csr_y = saved_csr_y;
    attrib = saved_attrib;
    vgamem = (uint16_t *)(0xB8000 + KERNEL_VIRTUAL_BASE);

    /* Restore hardware cursor position */
    uint32_t temp = csr_y * 80 + csr_x;
    outportb(0x3D4, 14);
    outportb(0x3D5, temp >> 8);
    outportb(0x3D4, 15);
    outportb(0x3D5, temp);
}