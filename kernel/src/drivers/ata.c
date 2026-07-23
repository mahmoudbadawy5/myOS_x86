#include <drivers/ata.h>
#include <arch.h>
#include <stdio.h>
#include <string.h>

static ata_device_t ata_primary;

static void ata_delay(uint16_t port)
{
    inportb(port + ATA_REG_STATUS);
    inportb(port + ATA_REG_STATUS);
    inportb(port + ATA_REG_STATUS);
    inportb(port + ATA_REG_STATUS);
}

static uint8_t ata_wait_ready(uint16_t port)
{
    uint8_t status;
    uint32_t timeout = 500000;
    while (timeout-- > 0) {
        status = inportb(port + ATA_REG_STATUS);
        if (!(status & ATA_SR_BSY))
            break;
    }
    return status;
}

static uint8_t ata_wait_drq(uint16_t port)
{
    uint8_t status;
    uint32_t timeout = 500000;
    while (timeout-- > 0) {
        status = inportb(port + ATA_REG_STATUS);
        if (status & ATA_SR_DRQ)
            break;
        if (status & ATA_SR_ERR)
            break;
    }
    return status;
}

void ata_init(void)
{
    ata_primary.base_port = ATA_PRIMARY_IO;
    ata_primary.ctrl_port = ATA_PRIMARY_CTRL;
    ata_primary.slave = 0;
    ata_primary.present = false;

    outportb(ATA_PRIMARY_CTRL, 0x06);
    ata_delay(ATA_PRIMARY_IO);

    outportb(ATA_PRIMARY_IO + ATA_REG_DRIVE_HEAD, 0xA0);
    ata_delay(ATA_PRIMARY_IO);

    outportb(ATA_PRIMARY_IO + ATA_REG_SECCOUNT, 0);
    outportb(ATA_PRIMARY_IO + ATA_REG_LBA_LO, 0);
    outportb(ATA_PRIMARY_IO + ATA_REG_LBA_MID, 0);
    outportb(ATA_PRIMARY_IO + ATA_REG_LBA_HI, 0);

    outportb(ATA_PRIMARY_IO + ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
    ata_delay(ATA_PRIMARY_IO);

    uint8_t status = inportb(ATA_PRIMARY_IO + ATA_REG_STATUS);
    if (status == 0) {
        return;
    }

    status = ata_wait_ready(ATA_PRIMARY_IO);
    if (status & ATA_SR_ERR) {
        return;
    }
    if (status & ATA_SR_BSY) {
        return;
    }

    uint8_t mid = inportb(ATA_PRIMARY_IO + ATA_REG_LBA_MID);
    uint8_t hi = inportb(ATA_PRIMARY_IO + ATA_REG_LBA_HI);
    if (mid != 0 || hi != 0) {
        return;
    }

    uint16_t identify_data[256];
    for (int i = 0; i < 256; i++) {
        identify_data[i] = inportw(ATA_PRIMARY_IO + ATA_REG_DATA);
    }

    ata_primary.present = true;
    ata_primary.sectors = *(uint32_t *)&identify_data[60];

    printf("[ATA] Primary master: %ux sectors\n", ata_primary.sectors);
}

ata_device_t *ata_get_device(void)
{
    return &ata_primary;
}

bool ata_read_sectors(uint32_t lba, uint32_t count, uint8_t *buffer)
{
    if (!ata_primary.present)
        return false;

    uint16_t port = ATA_PRIMARY_IO;

    ata_wait_ready(port);

    outportb(port + ATA_REG_DRIVE_HEAD, 0xE0 | ((lba >> 24) & 0x0F));
    outportb(port + ATA_REG_SECCOUNT, (uint8_t)count);
    outportb(port + ATA_REG_LBA_LO, (uint8_t)(lba & 0xFF));
    outportb(port + ATA_REG_LBA_MID, (uint8_t)((lba >> 8) & 0xFF));
    outportb(port + ATA_REG_LBA_HI, (uint8_t)((lba >> 16) & 0xFF));
    outportb(port + ATA_REG_COMMAND, ATA_CMD_READ_PIO);

    for (uint32_t s = 0; s < count; s++) {
        uint8_t status = ata_wait_drq(port);
        if (status & ATA_SR_ERR)
            return false;
        if (!(status & ATA_SR_DRQ))
            return false;

        for (int i = 0; i < 256; i++) {
            uint16_t word = inportw(port + ATA_REG_DATA);
            buffer[s * ATA_SECTOR_SIZE + i * 2] = (uint8_t)(word & 0xFF);
            buffer[s * ATA_SECTOR_SIZE + i * 2 + 1] = (uint8_t)((word >> 8) & 0xFF);
        }
    }
    return true;
}

bool ata_write_sectors(uint32_t lba, uint32_t count, const uint8_t *buffer)
{
    if (!ata_primary.present)
        return false;

    uint16_t port = ATA_PRIMARY_IO;

    ata_wait_ready(port);

    outportb(port + ATA_REG_DRIVE_HEAD, 0xE0 | ((lba >> 24) & 0x0F));
    outportb(port + ATA_REG_SECCOUNT, (uint8_t)count);
    outportb(port + ATA_REG_LBA_LO, (uint8_t)(lba & 0xFF));
    outportb(port + ATA_REG_LBA_MID, (uint8_t)((lba >> 8) & 0xFF));
    outportb(port + ATA_REG_LBA_HI, (uint8_t)((lba >> 16) & 0xFF));
    outportb(port + ATA_REG_COMMAND, ATA_CMD_WRITE_PIO);

    for (uint32_t s = 0; s < count; s++) {
        uint8_t status = ata_wait_drq(port);
        if (status & ATA_SR_ERR)
            return false;
        if (!(status & ATA_SR_DRQ))
            return false;

        for (int i = 0; i < 256; i++) {
            uint16_t word = buffer[s * ATA_SECTOR_SIZE + i * 2]
                          | (buffer[s * ATA_SECTOR_SIZE + i * 2 + 1] << 8);
            outportw(port + ATA_REG_DATA, word);
        }

        ata_wait_ready(port);
    }
    return true;
}
