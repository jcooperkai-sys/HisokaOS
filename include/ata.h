/* ata.h - ATA (IDE) PIO disk driver. Reads/writes 512-byte sectors on the primary
 * bus, master drive. This is real persistent block storage. */
#ifndef HISOKA_ATA_H
#define HISOKA_ATA_H
#include "types.h"

void ata_init(void);                                       /* probe the drive (IDENTIFY) */
int  ata_present(void);                                    /* 1 if a drive answered      */
int  ata_read (uint32_t lba, uint8_t count, void *buf);    /* read  count sectors @ lba  */
int  ata_write(uint32_t lba, uint8_t count, const void *buf);

#endif
