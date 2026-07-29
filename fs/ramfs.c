/* ramfs.c - in-memory filesystem with a directory tree.
 *
 * Nodes are kept in a flat array but each stores its full absolute path, so the
 * tree is implicit in the names ("/", "/home", "/home/file"). Creating a file or
 * directory auto-creates any missing parent directories, so the namespace always
 * stays consistent for `ls`/`cd`. */
#include "ramfs.h"
#include "heap.h"
#include "string.h"

static fs_file_t files[FS_MAX_FILES];

void fs_init(void) { memset(files, 0, sizeof(files)); }

fs_file_t *fs_at(int i) { return (i >= 0 && i < FS_MAX_FILES) ? &files[i] : NULL; }

fs_file_t *fs_find(const char *name) {
    for (int i = 0; i < FS_MAX_FILES; i++)
        if (files[i].used && !strcmp(files[i].name, name)) return &files[i];
    return NULL;
}

int fs_isdir(const char *path) {
    if (!strcmp(path, "/")) return 1;                 /* root is always a directory */
    fs_file_t *f = fs_find(path);
    return f && f->is_dir;
}

static fs_file_t *alloc_slot(const char *name) {
    for (int i = 0; i < FS_MAX_FILES; i++)
        if (!files[i].used) {
            memset(&files[i], 0, sizeof(files[i]));
            files[i].used = 1;
            size_t n = strlen(name); if (n >= FS_NAME_LEN) n = FS_NAME_LEN - 1;
            memcpy(files[i].name, name, n); files[i].name[n] = 0;
            return &files[i];
        }
    return NULL;
}

/* make sure every ancestor directory of `path` exists */
static void ensure_parents(const char *path) {
    char buf[FS_NAME_LEN];
    int n = (int)strlen(path);
    for (int i = 1; i < n && i < FS_NAME_LEN - 1; i++)
        if (path[i] == '/') {
            memcpy(buf, path, i); buf[i] = 0;
            if (!fs_find(buf)) { fs_file_t *d = alloc_slot(buf); if (d) d->is_dir = 1; }
        }
}

int fs_mkdir(const char *path) {
    if (fs_find(path)) return -1;
    ensure_parents(path);
    fs_file_t *d = alloc_slot(path);
    if (!d) return -1;
    d->is_dir = 1;
    return 0;
}

int fs_create(const char *name) {
    if (fs_find(name)) return -1;
    ensure_parents(name);
    return alloc_slot(name) ? 0 : -1;
}

/* a hook fired after any successful write, so the OS can react to control files */
static void (*g_write_hook)(const char *name);
void fs_set_write_hook(void (*h)(const char *name)) { g_write_hook = h; }

int fs_write(const char *name, const void *data, size_t len) {
    fs_file_t *f = fs_find(name);
    if (!f) { ensure_parents(name); f = alloc_slot(name); }
    if (!f || f->is_dir) return -1;
    if (f->data) kfree(f->data);
    f->data = kmalloc(len ? len : 1);
    if (!f->data) { f->len = 0; return -1; }
    memcpy(f->data, data, len);
    f->len = len;
    if (g_write_hook) g_write_hook(name);
    return (int)len;
}

int fs_append(const char *name, const void *data, size_t len) {
    fs_file_t *f = fs_find(name);
    if (!f) return fs_write(name, data, len);
    if (f->is_dir) return -1;
    uint8_t *nb = kmalloc(f->len + len);
    if (!nb) return -1;
    if (f->data) { memcpy(nb, f->data, f->len); kfree(f->data); }
    memcpy(nb + f->len, data, len);
    f->data = nb;
    f->len += len;
    if (g_write_hook) g_write_hook(name);
    return (int)f->len;
}

/* rename/move `old` to `newp`, carrying any descendants (so moving a directory
 * re-paths everything under it). */
int fs_move(const char *old, const char *newp) {
    if (fs_find(newp)) return -1;
    int oldlen = (int)strlen(old), found = 0;
    for (int i = 0; i < FS_MAX_FILES; i++) {
        fs_file_t *f = &files[i];
        if (!f->used) continue;
        if (!strcmp(f->name, old) || (strncmp(f->name, old, oldlen) == 0 && f->name[oldlen] == '/')) {
            char nb[FS_NAME_LEN]; int n = 0;
            for (const char *p = newp; *p && n < FS_NAME_LEN - 1; p++) nb[n++] = *p;
            for (const char *r = f->name + oldlen; *r && n < FS_NAME_LEN - 1; r++) nb[n++] = *r;
            nb[n] = 0;
            strcpy(f->name, nb);
            found = 1;
        }
    }
    return found ? 0 : -1;
}

int fs_delete(const char *name) {
    fs_file_t *f = fs_find(name);
    if (!f) return -1;
    /* if it's a directory, recursively remove every descendant so we never
       leave orphaned nodes (findable, slot-eating, but invisible to ls). */
    if (f->is_dir) {
        int nl = (int)strlen(name);
        for (int i = 0; i < FS_MAX_FILES; i++) {
            fs_file_t *c = &files[i];
            if (c->used && strncmp(c->name, name, nl) == 0 && c->name[nl] == '/') {
                if (c->data) kfree(c->data);
                memset(c, 0, sizeof(*c));
            }
        }
    }
    if (f->data) kfree(f->data);
    memset(f, 0, sizeof(*f));
    return 0;
}

int fs_count(void) {
    int n = 0;
    for (int i = 0; i < FS_MAX_FILES; i++) if (files[i].used && !files[i].is_dir) n++;
    return n;
}
