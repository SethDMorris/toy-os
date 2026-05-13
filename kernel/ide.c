#include "ide.h"
#include "ports.h"

/* Primary ATA channel, master (QEMU first IDE disk). */
#define IDE_DATA    0x1F0
#define IDE_ERROR   0x1F1
#define IDE_SECCNT  0x1F2
#define IDE_LBALO   0x1F3
#define IDE_LBAMID  0x1F4
#define IDE_LBAHI   0x1F5
#define IDE_DRIVE   0x1F6
#define IDE_STATUS  0x1F7
#define IDE_CMD     0x1F7
#define IDE_CONTROL 0x3F6

#define ATA_CMD_READ  0x20
#define ATA_CMD_WRITE 0x30

static int ide_busy(void) {
    return (inb(IDE_STATUS) & 0x80) != 0;
}

static int ide_wait_ready(void) {
    for (int i = 0; i < 500000; i++) {
        if (!ide_busy())
            return 0;
        io_wait();
    }
    return -1;
}

static int ide_wait_drq(void) {
    for (int i = 0; i < 500000; i++) {
        uint8_t s = inb(IDE_STATUS);
        if (s & 0x01)
            return -2; /* ERR */
        if (s & 0x08)
            return 0; /* DRQ */
        io_wait();
    }
    return -3;
}

void ide_init(void) {
    /* Software reset on bus (both drives). */
    outb(IDE_CONTROL, 0x04);
    io_wait();
    io_wait();
    outb(IDE_CONTROL, 0);
    io_wait();
    io_wait();

    (void)ide_wait_ready();
}

int ide_read_sector(uint32_t lba, void *buf512) {
    uint16_t *w = (uint16_t *)buf512;

    if (ide_wait_ready() != 0)
        return -1;

    outb(IDE_SECCNT, 1);
    outb(IDE_LBALO, (uint8_t)(lba & 0xFF));
    outb(IDE_LBAMID, (uint8_t)((lba >> 8) & 0xFF));
    outb(IDE_LBAHI, (uint8_t)((lba >> 16) & 0xFF));
    outb(IDE_DRIVE, (uint8_t)(0xE0 | ((lba >> 24) & 0x0F)));
    outb(IDE_CMD, ATA_CMD_READ);

    if (ide_wait_drq() != 0)
        return -4;

    for (int i = 0; i < 256; i++)
        w[i] = inw(IDE_DATA);

    (void)inb(IDE_STATUS);
    return 0;
}

int ide_write_sector(uint32_t lba, const void *buf512) {
    const uint16_t *w = (const uint16_t *)buf512;

    if (ide_wait_ready() != 0)
        return -1;

    outb(IDE_SECCNT, 1);
    outb(IDE_LBALO, (uint8_t)(lba & 0xFF));
    outb(IDE_LBAMID, (uint8_t)((lba >> 8) & 0xFF));
    outb(IDE_LBAHI, (uint8_t)((lba >> 16) & 0xFF));
    outb(IDE_DRIVE, (uint8_t)(0xE0 | ((lba >> 24) & 0x0F)));
    outb(IDE_CMD, ATA_CMD_WRITE);

    if (ide_wait_drq() != 0)
        return -4;

    for (int i = 0; i < 256; i++)
        outw(IDE_DATA, w[i]);

    if (ide_wait_ready() != 0)
        return -5;

    (void)inb(IDE_STATUS);
    return 0;
}
