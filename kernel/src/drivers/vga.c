#include <vga.h>
#include <arch.h>
#include <string.h>
#include <types.h>
#include <fs/vfs.h>
#include <mem/malloc.h>
#include <stdio.h>

uint16_t *vgamem;
int attrib = 0x0F;
int csr_x = 0, csr_y = 0, col_next = 0;

void scroll(void)
{
    uint16_t blank = (attrib << 8) | ' ';

    if (csr_y >= 25)
    {
        uint32_t scroll_by = csr_y - 25 + 1;
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
    if (c == 0x08)
    {
        if (csr_x != 0)
            csr_x--;
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
    if (csr_x == 80)
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
    /*stdout_node = malloc(sizeof(fs_node_t));
    stdout_node->inode = 0;
    stdout_node->write = vga_write_fs;
    stdout_node->flags |= FS_CHARDEVICE;
    files_open[1] = malloc(sizeof(FILE));
    files_open[1]->file = stdout_node;
    files_open[1]->flags = FILE_WRITE;*/
}