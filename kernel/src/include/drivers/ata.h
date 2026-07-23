#pragma once

#include <types.h>

#define ATA_PRIMARY_IO      0x1F0
#define ATA_PRIMARY_CTRL    0x3F6

#define ATA_REG_DATA        0x00
#define ATA_REG_ERROR       0x01
#define ATA_REG_SECCOUNT    0x02
#define ATA_REG_LBA_LO      0x03
#define ATA_REG_LBA_MID     0x04
#define ATA_REG_LBA_HI      0x05
#define ATA_REG_DRIVE_HEAD  0x06
#define ATA_REG_STATUS      0x07
#define ATA_REG_COMMAND     0x07

#define ATA_CMD_READ_PIO    0x20
#define ATA_CMD_WRITE_PIO   0x30
#define ATA_CMD_IDENTIFY    0xEC

#define ATA_SR_BSY          0x80
#define ATA_SR_DRDY         0x40
#define ATA_SR_DRQ          0x08
#define ATA_SR_ERR          0x01

#define ATA_SECTOR_SIZE     512

typedef struct {
    uint16_t base_port;
    uint16_t ctrl_port;
    uint8_t  slave;       // 0 = master, 1 = slave
    bool     present;
    uint32_t sectors;     // total sectors (from IDENTIFY)
} ata_device_t;

void ata_init(void);
bool ata_read_sectors(uint32_t lba, uint32_t count, uint8_t *buffer);
bool ata_write_sectors(uint32_t lba, uint32_t count, const uint8_t *buffer);
ata_device_t *ata_get_device(void);
