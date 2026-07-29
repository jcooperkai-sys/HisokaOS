/* ata.c - ATA/IDE PIO driver for the primary bus, master drive.
 *
 * Real persistent storage: the CPU talks to the disk controller through I/O ports
 * 0x1F0-0x1F7, issuing READ/WRITE SECTORS commands and moving 256 16-bit words per
 * sector by polling the status register. This is how the OS keeps files across
 * reboots instead of losing them with the RAM. */
#include "ata.h"
#include "ports.h"

#define ATA_DATA    0x1F0
#define ATA_SECCNT  0x1F2
#define ATA_LBA0    0x1F3
#define ATA_LBA1    0x1F4
#define ATA_LBA2    0x1F5
#define ATA_DRIVE   0x1F6
#define ATA_CMD     0x1F7   /* write: command */
#define ATA_STATUS  0x1F7   /* read:  status  */
#define ATA_ALTSTAT 0x3F6

#define ST_BSY  0x80
#define ST_DRQ  0x08
#define ST_ERR  0x01
#define ST_DF   0x20

static int present = 0;

static void io_delay(void) { for (int i = 0; i < 4; i++) (void)inb(ATA_ALTSTAT); }  /* ~400 ns */

/* poll until the controller is ready to transfer a sector (BSY low, DRQ high) */
static int wait_drq(void) {
    for (int i = 0; i < 4000000; i++) {
        uint8_t s = inb(ATA_STATUS);
        if (s & (ST_ERR | ST_DF)) return 0;
        if (!(s & ST_BSY) && (s & ST_DRQ)) return 1;
    }
    return 0;
}

void ata_init(void) {
    present = 0;
    outb(ATA_DRIVE, 0xA0);                      /* select master */
    io_delay();
    outb(ATA_SECCNT, 0); outb(ATA_LBA0, 0); outb(ATA_LBA1, 0); outb(ATA_LBA2, 0);
    outb(ATA_CMD, 0xEC);                        /* IDENTIFY */
    io_delay();
    if (inb(ATA_STATUS) == 0) return;           /* no drive */
    for (int i = 0; i < 1000000 && (inb(ATA_STATUS) & ST_BSY); i++) { }
    if (inb(ATA_LBA1) || inb(ATA_LBA2)) return; /* not a plain ATA disk (e.g. ATAPI) */
    if (!wait_drq()) return;
    for (int i = 0; i < 256; i++) (void)inw(ATA_DATA);   /* consume IDENTIFY data */
    present = 1;
}

int ata_present(void) { return present; }

int ata_read(uint32_t lba, uint8_t count, void *buf) {
    if (!present) return -1;
    outb(ATA_DRIVE, 0xE0 | ((lba >> 24) & 0x0F));
    io_delay();
    outb(ATA_SECCNT, count);
    outb(ATA_LBA0, lba & 0xFF); outb(ATA_LBA1, (lba >> 8) & 0xFF); outb(ATA_LBA2, (lba >> 16) & 0xFF);
    outb(ATA_CMD, 0x20);                        /* READ SECTORS */
    uint16_t *p = (uint16_t *)buf;
    int n = count ? count : 256;
    for (int s = 0; s < n; s++) {
        if (!wait_drq()) return -2;
        for (int i = 0; i < 256; i++) p[i] = inw(ATA_DATA);
        p += 256;
        io_delay();
    }
    return 0;
}

int ata_write(uint32_t lba, uint8_t count, const void *buf) {
    if (!present) return -1;
    outb(ATA_DRIVE, 0xE0 | ((lba >> 24) & 0x0F));
    io_delay();
    outb(ATA_SECCNT, count);
    outb(ATA_LBA0, lba & 0xFF); outb(ATA_LBA1, (lba >> 8) & 0xFF); outb(ATA_LBA2, (lba >> 16) & 0xFF);
    outb(ATA_CMD, 0x30);                        /* WRITE SECTORS */
    const uint16_t *p = (const uint16_t *)buf;
    int n = count ? count : 256;
    for (int s = 0; s < n; s++) {
        if (!wait_drq()) return -2;
        for (int i = 0; i < 256; i++) outw(ATA_DATA, p[i]);
        p += 256;
        io_delay();
    }
    outb(ATA_CMD, 0xE7);                         /* FLUSH CACHE */
    for (int i = 0; i < 1000000 && (inb(ATA_STATUS) & ST_BSY); i++) { }
    return 0;
}
