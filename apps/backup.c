/* backup.c - the Backups app. Snapshots all your /home files into one archive file
 * under /backups, keeps several (redundancy = "backups for backups"), and restores.
 * Everything stays in the filesystem (persisted by 'sync'); no risky disk surgery. */
#include "backup.h"
#include "vga.h"
#include "keyboard.h"
#include "printf.h"
#include "ramfs.h"
#include "pit.h"
#include "ui.h"
#include "string.h"
#include "types.h"

static char archive[65536];

/* serialize all /home files into the archive; returns bytes (0 on failure) */
static int snapshot_make(const char *path) {
    int o = 0;
    for (int i = 0; i < FS_MAX_FILES; i++) {
        fs_file_t *f = fs_at(i);
        if (!f || !f->used || f->is_dir) continue;
        if (strncmp(f->name, "/home/", 6)) continue;
        int nl = (int)strlen(f->name);
        if (o + 6 + nl + (int)f->len > (int)sizeof(archive) - 8) break;
        archive[o++] = (char)(nl & 0xFF); archive[o++] = (char)((nl >> 8) & 0xFF);
        for (int k = 0; k < nl; k++) archive[o++] = f->name[k];
        uint32_t dl = (uint32_t)f->len;
        archive[o++] = (char)(dl & 0xFF); archive[o++] = (char)((dl>>8)&0xFF); archive[o++] = (char)((dl>>16)&0xFF); archive[o++] = (char)((dl>>24)&0xFF);
        for (size_t k = 0; k < f->len; k++) archive[o++] = (char)f->data[k];
    }
    fs_mkdir("/backups");
    return fs_write(path, archive, (size_t)o) >= 0 ? o : 0;
}
static int snapshot_restore(const char *path) {
    fs_file_t *f = fs_find(path);
    if (!f || !f->data) return 0;
    uint8_t *d = f->data; int len = (int)f->len, o = 0, count = 0;
    while (o + 6 <= len) {
        int nl = d[o] | (d[o+1] << 8); o += 2;
        if (nl <= 0 || nl >= FS_NAME_LEN || o + nl + 4 > len) break;
        char name[FS_NAME_LEN]; for (int k = 0; k < nl; k++) name[k] = (char)d[o+k]; name[nl] = 0; o += nl;
        uint32_t dl = d[o] | (d[o+1]<<8) | (d[o+2]<<16) | ((uint32_t)d[o+3]<<24); o += 4;
        if (o + (int)dl > len) break;
        fs_write(name, d + o, dl); o += dl; count++;
    }
    return count;
}

/* list the .bak files under /backups into out[]; returns count */
static int list_backups(char out[32][FS_NAME_LEN]) {
    int n = 0;
    for (int i = 0; i < FS_MAX_FILES && n < 32; i++) {
        fs_file_t *f = fs_at(i);
        if (!f || !f->used || f->is_dir) continue;
        if (strncmp(f->name, "/backups/", 9)) continue;
        int len = (int)strlen(f->name);
        if (len > 4 && !strcmp(f->name + len - 4, ".bak")) strcpy(out[n++], f->name);
    }
    return n;
}
static const char *bk_base(const char *p) { const char *b = p; for (const char *q = p; *q; q++) if (*q == '/') b = q + 1; return b; }

void backup_run(void) {
    static char names[32][FS_NAME_LEN];
    int sel = 0; char msg[48] = {0}; uint8_t mc = VGA_LGREEN;
    for (;;) {
        int n = list_backups(names);
        if (sel >= n) sel = n ? n - 1 : 0;
        vga_clear();
        ui_panel(2, 1, 76, 22, "Backups", VGA_LCYAN, VGA_BLACK);
        ui_text(5, 3, "Snapshots of your /home files (newest kept).", VGA_DGREY, VGA_BLACK);
        if (!n) ui_text(6, 5, "No backups yet. Press 'n' to make one.", VGA_DGREY, VGA_BLACK);
        for (int i = 0; i < n; i++) {
            int y = 5 + i; if (y > 19) break;
            uint8_t fg=(i==sel)?VGA_BLACK:VGA_LGREY, bg=(i==sel)?VGA_LGREY:VGA_BLACK;
            for (int x=4;x<=72;x++) vga_cell(x,y,' ',fg,bg);
            int x=5; for (const char *p=bk_base(names[i]); *p && x<72; p++) vga_cell(x++,y,*p,fg,bg);
        }
        if (msg[0]) ui_text(5, 21, msg, mc, VGA_BLACK);
        vga_statusbar(" BACKUPS  n new  Enter/r restore  d delete  q quit");
        vga_setcursor(0, 24);
        char k = keyboard_getc();
        if (k == 'q' || k == 27) break;
        else if (k == 0x10) { if (sel > 0) sel--; }
        else if (k == 0x0E) { if (sel < n-1) sel++; }
        else if (k == 'n') {
            char path[FS_NAME_LEN]; int o = 0; for (const char *p = "/backups/snap-"; *p; p++) path[o++] = *p;
            uint32_t v = pit_ticks(); char t[12]; int m = 0; if (!v) t[m++]='0'; while (v) { t[m++]=(char)('0'+v%10); v/=10; } while (m) path[o++]=t[--m];
            for (const char *p = ".bak"; *p; p++) path[o++] = *p; path[o] = 0;
            int bytes = snapshot_make(path);
            if (bytes) { int q=0; for(const char*p="Backup created (";*p;p++)msg[q++]=*p; int vv=bytes; char tt[8]; int mm=0; if(!vv)tt[mm++]='0'; while(vv){tt[mm++]=(char)('0'+vv%10);vv/=10;} while(mm)msg[q++]=tt[--mm]; for(const char*p=" bytes)";*p;p++)msg[q++]=*p; msg[q]=0; mc=VGA_LGREEN; }
            else { strcpy(msg, "backup failed (filesystem full?)"); mc = VGA_LRED; }
        }
        else if (k == '\n' || k == 'r') {
            if (n) { int c = snapshot_restore(names[sel]); int q=0; for(const char*p="Restored ";*p;p++)msg[q++]=*p; char tt[8]; int mm=0,vv=c; if(!vv)tt[mm++]='0'; while(vv){tt[mm++]=(char)('0'+vv%10);vv/=10;} while(mm)msg[q++]=tt[--mm]; for(const char*p=" files.";*p;p++)msg[q++]=*p; msg[q]=0; mc=VGA_LCYAN; }
        }
        else if (k == 'd') { if (n) { fs_delete(names[sel]); if (sel>0) sel--; strcpy(msg, "Backup deleted."); mc = VGA_DGREY; } }
    }
    vga_clear(); vga_setcolor(VGA_LCYAN, VGA_BLACK); kputs("Backups closed (run 'sync' to keep them on disk).\n"); vga_setcolor(VGA_LGREY, VGA_BLACK);
}
