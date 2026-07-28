#include <types.h>
#include <arch.h>
#include <arch/bga.h>

/* BGA I/O ports */
#define BGA_INDEX   0x01CE
#define BGA_DATA    0x01CF

/* BGA register indices */
#define BGA_ID          0
#define BGA_XRES        1
#define BGA_YRES        2
#define BGA_BPP         3
#define BGA_ENABLE      4
#define BGA_BANK        5
#define BGA_VIRT_WIDTH  6
#define BGA_VIRT_HEIGHT 7
#define BGA_X_OFFSET    8
#define BGA_Y_OFFSET    9

/* BGA enable values */
#define BGA_DISABLED        0x00
#define BGA_ENABLED         0x01
#define BGA_GETCAPS         0x02
#define BGA_8BIT_DAC        0x20
#define BGA_LFB_ENABLED     0x40
#define BGA_NOCLEARMEM      0x80

/* QEMU VGA PCI: vendor 0x1234, device 0x1111, BAR0 = LFB phys addr */
#define QEMU_VGA_PCI_VENDOR  0x1234
#define QEMU_VGA_PCI_DEVICE  0x1111

static uint32_t bga_lfb_phys = 0;
static uint32_t bga_pitch = 0;
static uint32_t bga_width = 0;
static uint32_t bga_height = 0;

static void bga_write(uint16_t index, uint16_t value)
{
    outportw(BGA_INDEX, index);
    outportw(BGA_DATA, value);
}

static uint16_t bga_read(uint16_t index)
{
    outportw(BGA_INDEX, index);
    return inportw(BGA_DATA);
}

#define BGA_VERSION         0xB0C5

static int bga_detect(void)
{
    bga_write(BGA_ID, BGA_VERSION);
    return (bga_read(BGA_ID) == BGA_VERSION);
}

/* PCI config read (simplified: only for QEMU VGA at bus 0 dev 2 func 0) */
static uint32_t pci_read32(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset)
{
    uint32_t addr = (1U << 31)                  /* enable bit */
                  | ((uint32_t)bus << 16)
                  | ((uint32_t)(dev & 0x1F) << 11)
                  | ((uint32_t)(func & 0x07) << 8)
                  | (offset & 0xFC);
    outportl(0xCF8, addr);
    return inportl(0xCFC);
}

static void bga_probe_lfb(void)
{
    /* Try to find QEMU VGA (0x1234:0x1111) on PCI bus 0 */
    for (uint8_t dev = 0; dev < 32; dev++) {
        uint32_t vendor_device = pci_read32(0, dev, 0, 0);
        uint16_t vendor = vendor_device & 0xFFFF;
        uint16_t device = (vendor_device >> 16) & 0xFFFF;
        if (vendor == QEMU_VGA_PCI_VENDOR && device == QEMU_VGA_PCI_DEVICE) {
            /* BAR0 = LFB physical address */
            uint32_t bar0 = pci_read32(0, dev, 0, 0x10);
            /* Ensure BAR0 is a valid memory address */
            if ((bar0 & 1) == 0 && (bar0 & 0xFFFFFFF0) != 0) {
                bga_lfb_phys = bar0 & 0xFFFFFFF0;
                return;
            }
        }
    }
    /* Fallback: QEMU standard VGA LFB at 0xFD000000 */
    bga_lfb_phys = 0xFD000000;
}

extern void vga_save_font(void);
extern void vga_save_cursor_state(void);

int bga_set_mode(uint16_t width, uint16_t height, uint16_t bpp)
{
    if (!bga_detect())
        return -1;

    /* Save VGA font before BGA destroys VGA plane data (only once) */
    extern int font_saved;
    if (!font_saved)
        vga_save_font();

    /* Save cursor state on every mode switch */
    vga_save_cursor_state();

    bga_probe_lfb();

    /* Disable BGA before changing resolution */
    bga_write(BGA_ENABLE, BGA_DISABLED);

    /* Set resolution and depth */
    bga_write(BGA_XRES, width);
    bga_write(BGA_YRES, height);
    bga_write(BGA_BPP, bpp);

    /* Enable with linear framebuffer */
    bga_write(BGA_ENABLE, BGA_ENABLED | BGA_LFB_ENABLED | BGA_NOCLEARMEM);

    /* Verify it took */
    if (bga_read(BGA_XRES) != width || bga_read(BGA_YRES) != height)
        return -2;

    bga_width = width;
    bga_height = height;
    bga_pitch = width * ((bpp + 7) / 8);

    return 0;
}

uint32_t bga_get_lfb_addr(void) { return bga_lfb_phys; }
uint32_t bga_get_pitch(void)    { return bga_pitch; }
uint32_t bga_get_width(void)    { return bga_width; }
uint32_t bga_get_height(void)   { return bga_height; }

void bga_disable(void)
{
    /* Always try to disable — don't rely on detect after mode set */
    outportw(BGA_INDEX, BGA_ENABLE);
    outportw(BGA_DATA, BGA_DISABLED);
}
