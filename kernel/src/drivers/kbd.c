#include <kbd.h>
#include <irq.h>
#include <vga.h>
#include <arch.h>
#include <mem/malloc.h>
#include <stdio.h>
#include <fs/vfs.h>
#include <proc/process.h>

/* PS/2 keyboard scancodes (make codes, set 1) */
#define SCANCODE_ESC       0x01
#define SCANCODE_BACKSPACE 0x0E
#define SCANCODE_TAB       0x0F
#define SCANCODE_ENTER     0x1C
#define SCANCODE_CTRL_L    0x1D
#define SCANCODE_SHIFT_L   0x2A
#define SCANCODE_SHIFT_R   0x36
#define SCANCODE_ALT_L     0x38
#define SCANCODE_CAPSLOCK  0x3A
#define SCANCODE_F1        0x3B
#define SCANCODE_F10       0x44
#define SCANCODE_SCROLLLOCK 0x46
#define SCANCODE_HOME      0x47
#define SCANCODE_UP        0x48
#define SCANCODE_PAGEUP    0x49
#define SCANCODE_LEFT      0x4B
#define SCANCODE_RIGHT     0x4D
#define SCANCODE_END       0x4F
#define SCANCODE_DOWN      0x50
#define SCANCODE_PAGEDOWN  0x51
#define SCANCODE_INSERT    0x52
#define SCANCODE_DELETE    0x53
#define SCANCODE_F11       0x57
#define SCANCODE_F12       0x58

/* Ctrl+key scancodes */
#define SCANCODE_KEY_C     0x2E
#define SCANCODE_KEY_Z     0x2C

/* KBDUS means US Keyboard Layout. This is a scancode table
 *  used to layout a standard US keyboard. I have left some
 *  comments in to give you an idea of what key is what, even
 *  though I set it's array index to 0. You can change that to
 *  whatever you want using a macro, if you wish! */
unsigned char kbdus[2][128] = {
    {
        0, 27, '1', '2', '3', '4', '5', '6', '7', '8',    /* 9 */
        '9', '0', '-', '=', '\b',                         /* Backspace */
        '\t',                                             /* Tab */
        'q', 'w', 'e', 'r',                               /* 19 */
        't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',     /* Enter key */
        0,                                                /* 29   - Control */
        'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', /* 39 */
        '\'', '`', 0,                                     /* Left shift */
        '\\', 'z', 'x', 'c', 'v', 'b', 'n',               /* 49 */
        'm', ',', '.', '/', 0,                            /* Right shift */
        '*',
        0,   /* Alt */
        ' ', /* Space bar */
        0,   /* Caps lock */
        0,   /* 59 - F1 key ... > */
        0, 0, 0, 0, 0, 0, 0, 0,
        0, /* < ... F10 */
        0, /* 69 - Num lock*/
        0, /* Scroll Lock */
        0, /* Home key */
        0, /* Up Arrow */
        0, /* Page Up */
        '-',
        0, /* Left Arrow */
        0,
        0, /* Right Arrow */
        '+',
        0, /* 79 - End key*/
        0, /* Down Arrow */
        0, /* Page Down */
        0, /* Insert Key */
        0, /* Delete Key */
        0, 0, 0,
        0, /* F11 Key */
        0, /* F12 Key */
        0, /* All other keys are undefined */
    },
    {
        0, 27, '!', '@', '#', '$', '%', '^', '&', '*',    /* 9 */
        '(', ')', '_', '+', '\b',                         /* Backspace */
        '\t',                                             /* Tab */
        'Q', 'W', 'E', 'R',                               /* 19 */
        'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',     /* Enter key */
        0,                                                /* 29   - Control */
        'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', /* 39 */
        '"', '~', 0,                                      /* Left shift */
        '|', 'Z', 'X', 'C', 'V', 'B', 'N',                /* 49 */
        'M', '<', '>', '?', 0,                            /* Right shift */
        '*',
        0,   /* Alt */
        ' ', /* Space bar */
        0,   /* Caps lock */
        0,   /* 59 - F1 key ... > */
        0, 0, 0, 0, 0, 0, 0, 0,
        0, /* < ... F10 */
        0, /* 69 - Num lock*/
        0, /* Scroll Lock */
        0, /* Home key */
        0, /* Up Arrow */
        0, /* Page Up */
        '-',
        0, /* Left Arrow */
        0,
        0, /* Right Arrow */
        '+',
        0, /* 79 - End key*/
        0, /* Down Arrow */
        0, /* Page Down */
        0, /* Insert Key */
        0, /* Delete Key */
        0, 0, 0,
        0, /* F11 Key */
        0, /* F12 Key */
        0, /* All other keys are undefined */
    }};

volatile int shift = 0, caps_lock = 0, ctrl = 0;
char *kbd_buf;
volatile uint32_t buf_start=0, buf_size=0;

/* Handles the keyboard interrupt */
void keyboard_handler(struct regs *r)
{
    unsigned char scancode;

    scancode = inportb(0x60);

    if (scancode & 0x80)
    {
        /* Key release: release codes = make code | 0x80 */
        uint8_t release = scancode & 0x7F;
        if (release == SCANCODE_SHIFT_L || release == SCANCODE_SHIFT_R)
            shift = 0;
        else if (release == SCANCODE_CTRL_L)
            ctrl = 0;
    }
    else
    {
        if (scancode == SCANCODE_SHIFT_L || scancode == SCANCODE_SHIFT_R)
            shift = 1;
        if (scancode == SCANCODE_CTRL_L)
            ctrl = 1;
        if (scancode == SCANCODE_CAPSLOCK)
            caps_lock = !caps_lock;

        /* Ctrl+C: send SIGINT (signal 2) to foreground process group */
        if (ctrl && scancode == SCANCODE_KEY_C)
        {
            if (foreground_pgid) {
                for (int i = 0; i < MAX_PROCESSES; i++) {
                    if (process_table[i].pgid == foreground_pgid &&
                        process_table[i].state != PROCESS_STATE_TERMINATED) {
                        process_table[i].signal_pending |= SIG_BIT(SIGINT);
                    }
                }
            }
            outportb(0x20, 0x20);
            schedule(r);
            return;
        }

        /* Ctrl+Z: send SIGTSTP (signal 20) to foreground process group */
        if (ctrl && scancode == SCANCODE_KEY_Z)
        {
            if (foreground_pgid) {
                for (int i = 0; i < MAX_PROCESSES; i++) {
                    if (process_table[i].pgid == foreground_pgid &&
                        process_table[i].state != PROCESS_STATE_TERMINATED) {
                        process_table[i].signal_pending |= SIG_BIT(SIGTSTP);
                    }
                }
            }
            outportb(0x20, 0x20);
            schedule(r);
            return;
        }

        char ret = kbdus[shift][scancode];
        if (!ret)
            return;
        if ('a' <= ret && ret <= 'z')
        {
            if (caps_lock)
                ret = ret - 'a' + 'A';
        }
        else if ('A' <= ret && ret <= 'Z')
        {
            if (caps_lock)
                ret = ret - 'A' + 'a';
        }
        if (buf_size >= KBD_BUFFER_SZ)
            return;
        kbd_buf[(buf_start + buf_size) & (KBD_BUFFER_SZ - 1)] = ret;
        buf_size++;
    }
}

uint32_t keyboard_read_fs(fs_node_t *node, uint32_t size, uint32_t units, uint8_t *buffer)
{
    uint32_t total_read = 0;
    for (int i = 0; i < size * units; i++)
    {
        if(!buf_size) break;
        buffer[i] = kbd_buf[buf_start];
        buf_start = (buf_start + 1)&(KBD_BUFFER_SZ - 1);
        buf_size--; total_read++;
    }
    node->seek_offset += total_read;
    return total_read;
}


void keyboard_install()
{
    /* Flush any stale data from the PS/2 output buffer */
    while (inportb(0x64) & 1)
        inportb(0x60);

    /* Enable keyboard scanning on the PS/2 controller */
    outportb(0x64, 0xAE);

    kbd_buf = (char *)malloc(KBD_BUFFER_SZ);
    buf_start = 0;
    buf_size = 0;
    stdin_node = malloc(sizeof(fs_node_t));
    stdin_node->inode = 0;
    stdin_node->read = keyboard_read_fs;
    stdin_node->flags |= FS_CHARDEVICE;
    irq_install_handler(1, keyboard_handler);
}