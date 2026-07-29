/* persist.h - save/restore the ramfs to the ATA disk so files survive reboots. */
#ifndef HISOKA_PERSIST_H
#define HISOKA_PERSIST_H
int persist_load(void);   /* restore files from disk at boot; returns count, <0 on error */
int persist_save(void);   /* write all files to disk; 0 ok, <0 on error */
int persist_format(void); /* wipe the on-disk image (factory reset); 0 ok, <0 on error */
#endif
