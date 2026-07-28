#include <mouse.h>
#include <irq.h>
#include <arch.h>
#include <mem/malloc.h>
#include <fs/vfs.h>
#include <string.h>
#include <stdio.h>

static uint8_t mouse_buf[MOUSE_BUFFER_SZ];
static volatile uint32_t mouse_start = 0, mouse_size = 0;

static fs_node_t *mouse_node = NULL;

static uint8_t mouse_cycle = 0;
static uint8_t mouse_packet[3];

static void ps2_wait_input(void)
{
    for (int i = 0; i < 10000; i++)
        if (!(inportb(0x64) & 2))
            return;
}

static void ps2_wait_output(void)
{
    for (int i = 0; i < 10000; i++)
        if (inportb(0x64) & 1)
            return;
}

static void ps2_write_cmd(uint8_t cmd)
{
    ps2_wait_input();
    outportb(0x64, cmd);
}

static void ps2_write_cmd_data(uint8_t data)
{
    ps2_wait_input();
    outportb(0x60, data);
}

static void ps2_write_data(uint8_t data)
{
    ps2_wait_input();
    outportb(0x64, 0xD4);
    ps2_wait_input();
    outportb(0x60, data);
}

static uint8_t ps2_read(void)
{
    ps2_wait_output();
    return inportb(0x60);
}

static void mouse_handler(struct regs *r)
{
    (void)r;

    uint8_t status = inportb(0x64);
    if (!(status & 0x01) || !(status & 0x20))
        goto eoi;

    uint8_t byte = inportb(0x60);

    switch (mouse_cycle) {
        case 0:
            if (byte & 0x08) {
                mouse_packet[0] = byte;
                mouse_cycle = 1;
            }
            break;
        case 1:
            mouse_packet[1] = byte;
            mouse_cycle = 2;
            break;
        case 2:
            mouse_packet[2] = byte;
            mouse_cycle = 0;
            for (int i = 0; i < 3; i++) {
                if (mouse_size < MOUSE_BUFFER_SZ) {
                    mouse_buf[(mouse_start + mouse_size) & (MOUSE_BUFFER_SZ - 1)] = mouse_packet[i];
                    mouse_size++;
                }
            }
            break;
    }

eoi:
    outportb(0xA0, 0x20);
    outportb(0x20, 0x20);
}

static uint32_t mouse_read_fs(fs_node_t *node, uint32_t size, uint32_t units, uint8_t *buffer)
{
    (void)size;
    uint32_t total_read = 0;
    uint32_t requested = units;
    while (requested > 0 && mouse_size >= 3) {
        for (int i = 0; i < 3 && requested > 0; i++) {
            buffer[total_read] = mouse_buf[mouse_start];
            mouse_start = (mouse_start + 1) & (MOUSE_BUFFER_SZ - 1);
            mouse_size--;
            total_read++;
            requested--;
        }
    }
    node->seek_offset += total_read;
    return total_read;
}

fs_node_t *mouse_get_node(void)
{
    return mouse_node;
}

void mouse_install(void)
{
    irq_install_handler(12, mouse_handler);

    while (inportb(0x64) & 1)
        inportb(0x60);

    ps2_write_cmd(0xA8);

    ps2_write_data(0xFF);
    uint8_t ack = ps2_read();
    uint8_t bat = ps2_read();
    uint8_t id  = ps2_read();
    printf("[MOUSE] reset ACK=0x%02x BAT=0x%02x ID=0x%02x\n", ack, bat, id);

    ps2_write_data(0xF6);
    ps2_read();

    ps2_write_data(0xF4);
    ps2_read();

    ps2_write_cmd(0x20);
    uint8_t config = ps2_read();
    config |= 0x02;
    config &= ~0x20;
    ps2_write_cmd(0x60);
    ps2_write_cmd_data(config);

    mouse_node = malloc(sizeof(fs_node_t));
    memset((uint8_t *)mouse_node, 0, sizeof(fs_node_t));
    strcpy(mouse_node->name, "mouse");
    mouse_node->flags = FS_CHARDEVICE;
    mouse_node->read = mouse_read_fs;

    mount_entry_t *entry = malloc(sizeof(mount_entry_t));
    strcpy(entry->name, "mouse");
    entry->node = mouse_node;
    entry->next = root_dir->mounts;
    root_dir->mounts = entry;
}
