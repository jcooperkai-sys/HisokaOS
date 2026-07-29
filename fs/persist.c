/* persist.c - a tiny on-disk image of the ramfs.
 *
 * Layout (a flat blob written from sector 0): [magic u32]["HFS1"][count u32] then,
 * per file, [name 32 bytes][len u32][data len bytes]. persist_load() reads it back
 * at boot and recreates the files; persist_save() (the `sync` command) writes the
 * current files out. This is what makes HisokaOS remember your work across reboots. */
#include "persist.h"
#include "ata.h"
#include "ramfs.h"
#include "string.h"

#define MAGIC        0x31534648u   /* 'HFS1' */
#define BLOB_SECTORS 200           /* 100 KiB working area */
#define BLOB_BYTES   (BLOB_SECTORS * 512)

static uint8_t blob[BLOB_BYTES] __attribute__((aligned(4)));

static uint32_t rd32(const uint8_t *p) { return p[0] | (p[1]<<8) | (p[2]<<16) | ((uint32_t)p[3]<<24); }
static void     wr32(uint8_t *p, uint32_t v) { p[0]=v; p[1]=v>>8; p[2]=v>>16; p[3]=v>>24; }

int persist_save(void) {
    if (!ata_present()) return -1;
    wr32(blob, MAGIC);
    int off = 8;
    uint32_t count = 0;
    for (int i = 0; i < FS_MAX_FILES; i++) {
        fs_file_t *f = fs_at(i);
        if (!f || !f->used) continue;
        int dlen = f->is_dir ? 0 : (int)f->len;
        if (off + FS_NAME_LEN + 8 + dlen > BLOB_BYTES) break;
        memcpy(blob + off, f->name, FS_NAME_LEN); off += FS_NAME_LEN;
        wr32(blob + off, (uint32_t)f->is_dir);    off += 4;
        wr32(blob + off, (uint32_t)dlen);         off += 4;
        if (dlen) { memcpy(blob + off, f->data, dlen); off += dlen; }
        count++;
    }
    wr32(blob + 4, count);
    int sectors = (off + 511) / 512; if (sectors < 1) sectors = 1; if (sectors > BLOB_SECTORS) sectors = BLOB_SECTORS;
    return ata_write(0, (uint8_t)sectors, blob);
}

int persist_load(void) {
    if (!ata_present()) return -1;
    if (ata_read(0, BLOB_SECTORS, blob) != 0) return -2;
    if (rd32(blob) != MAGIC) return 0;            /* fresh/unformatted disk */
    uint32_t count = rd32(blob + 4);
    int off = 8, loaded = 0;
    for (uint32_t i = 0; i < count && i < FS_MAX_FILES; i++) {
        if (off + FS_NAME_LEN + 8 > BLOB_BYTES) break;
        char name[FS_NAME_LEN]; memcpy(name, blob + off, FS_NAME_LEN); name[FS_NAME_LEN-1] = 0; off += FS_NAME_LEN;
        uint32_t is_dir = rd32(blob + off); off += 4;
        uint32_t len    = rd32(blob + off); off += 4;
        if (len > (uint32_t)(BLOB_BYTES - off)) break;   /* unsigned: a corrupt >=2^31 len can't bypass this */
        if (is_dir) fs_mkdir(name);
        else        fs_write(name, blob + off, len);
        off += (int)len;
        loaded++;
    }
    return loaded;
}

/* wipe the on-disk image: clear the magic so the next boot sees a fresh disk and
 * reseeds defaults. Used by 'factory reset'. */
int persist_format(void) {
    if (!ata_present()) return -1;
    memset(blob, 0, BLOB_BYTES);
    return ata_write(0, 1, blob);
}
