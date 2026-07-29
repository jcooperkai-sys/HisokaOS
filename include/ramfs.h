/* ramfs.h - in-memory filesystem with a real directory tree. Each node stores its
 * full absolute path (e.g. "/home/Downloads/x.txt") plus an is_dir flag; parent
 * directories are created automatically. Files live in kmalloc'd buffers. */
#ifndef HISOKA_RAMFS_H
#define HISOKA_RAMFS_H
#include "types.h"

#define FS_MAX_FILES 192
#define FS_NAME_LEN  64

typedef struct {
    char     name[FS_NAME_LEN];     /* full absolute path */
    uint8_t *data;
    size_t   len;
    int      used;
    int      is_dir;
} fs_file_t;

void        fs_init(void);
int         fs_mkdir(const char *path);                  /* make a directory (and parents) */
int         fs_create(const char *path);                 /* 0 ok, -1 full/exists */
int         fs_write(const char *path, const void *data, size_t len);  /* create/overwrite */
int         fs_append(const char *path, const void *data, size_t len);
fs_file_t  *fs_find(const char *path);
int         fs_isdir(const char *path);
int         fs_move(const char *oldpath, const char *newpath);  /* rename/move a file or whole subtree */
int         fs_delete(const char *path);
int         fs_count(void);
fs_file_t  *fs_at(int i);                                /* iterate slots (may be unused) */
void        fs_set_write_hook(void (*h)(const char *name)); /* fired after any write (control files) */

#endif
