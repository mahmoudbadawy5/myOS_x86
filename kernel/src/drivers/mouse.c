#include <mouse.h>
#include <irq.h>
#include <arch.h>
#include <mem/malloc.h>
#include <fs/vfs.h>
#include <string.h>

/* PS/2 mouse circular buffer: each packet is3 bytes (dx, dy, buttons) */
static uint8_t mouse_buf[MOUSE_BUFFER_SZ];
static volatile uint32_t mouse_start = 0, mouse_size = 0;

static fs_node_t *mouse_node = NULL;

/* Wait for PS/2 controller input buffer to be empty */
static void ps2_wait_input(void)
{
    for (int i = 0; i < 10000; i++)
        if (!(inportb(0x64) & 2))
            return;
}

/* Wait for PS/2 controller output buffer to be full */
static void ps2_wait_output(void)
{
    for (int i = 0; i < 10000; i++)
        if (inportb(0x64) & 1)
            return;
}

/* Write a command to the PS/2 controller (port 0x64) */
static void ps2_write_cmd(uint8_t cmd)
{
    ps2_wait_input();
    outportb(0x64, cmd);
}

/* Write data byte to PS/2 controller (for mouse, preceded by0xD4) */
static void ps2_write_data(uint8_t data)
{
    ps2_wait_input();
    outportb(0x64, 0xD4); /* Tell controller next byte goes to auxiliary port */
    ps2_wait_input();
    outportb(0x60, data);
}

/* Read a response byte from PS/2 data port */
static uint8_t ps2_read(void)
{
    ps2_wait_output();
    return inportb(0x60);
}

/* IRQ12 handler: PS/2 auxiliary device (mouse) */
static void mouse_handler(struct regs *r)
{
    (void)r;

    /* Mouse sends3-byte packets. We read all available bytes. */
    uint8_t packet[3];
    for (int i = 0; i < 3; i++) {
        ps2_wait_output();
        packet[i] = inportb(0x60);
    }

    /* Enqueue all3 bytes */
    for (int i = 0; i < 3; i++) {
        if (mouse_size < MOUSE_BUFFER_SZ) {
            mouse_buf[(mouse_start + mouse_size) & (MOUSE_BUFFER_SZ - 1)] = packet[i];
            mouse_size++;
        }
    }

    /* Send EOI to both PICs (mouse is IRQ12, slave PIC) */
    outportb(0xA0, 0x20); /* EOI to slave PIC */
    outportb(0x20, 0x20); /* EOI to master PIC */
}

/* VFS read for /dev/mouse: returns mouse packets (3 bytes each) */
static uint32_t mouse_read_fs(fs_node_t *node, uint32_t size, uint32_t units, uint8_t *buffer)
{
    (void)size;
    uint32_t total_read = 0;
    uint32_t requested = units;
    while (requested > 0 && mouse_size > 0) {
        buffer[total_read] = mouse_buf[mouse_start];
        mouse_start = (mouse_start + 1) & (MOUSE_BUFFER_SZ - 1);
        mouse_size--;
        total_read++;
        requested--;
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
    /* Flush stale data from PS/2 output buffer */
    while (inportb(0x64) & 1)
        inportb(0x60);

    /* Step1: Enable auxiliary device (mouse) */
    ps2_write_cmd(0xA8);

    /* Step2: Enable IRQ12 in controller configuration byte */
    ps2_write_cmd(0x20);            /* Read configuration byte */
    uint8_t config = ps2_read();
    config |= 0x02;                 /* Bit1: enable IRQ12 (auxiliary) */
    config &= ~0x20;                /* Bit5: enable mouse clock */
    ps2_write_cmd(0x60);            /* Write configuration byte */
    ps2_write_data(config);

    /* Step3: Reset mouse (gets0xAA self-test pass response) */
    ps2_write_data(0xFF);
    ps2_read();                     /*0xAA = self-test pass */
    ps2_read();                     /*0x00 = mouse ID */

    /* Step4: Set defaults (sample rate100, resolution4 px/mm) */
    ps2_write_data(0xF6);           /* Set defaults */
    ps2_read();                     /* ACK */

    /* Step5: Enable data reporting */
    ps2_write_data(0xF4);           /* Enable data reporting */
    ps2_read();                     /* ACK */

    /* Create /dev/mouse VFS node */
    mouse_node = malloc(sizeof(fs_node_t));
    memset((uint8_t *)mouse_node, 0, sizeof(fs_node_t));
    strcpy(mouse_node->name, "mouse");
    mouse_node->flags = FS_CHARDEVICE;
    mouse_node->read = mouse_read_fs;

    /* Mount at /mouse in root directory */
    mount_entry_t *entry = malloc(sizeof(mount_entry_t));
    strcpy(entry->name, "mouse");
    entry->node = mouse_node;
    entry->next = root_dir->mounts;
    root_dir->mounts = entry;

    /* Install IRQ12 handler */
    irq_install_handler(12, mouse_handler);
}
