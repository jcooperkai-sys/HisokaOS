/* explorer.c - a full file manager for the directory tree.
 *
 * Navigate folders with the arrows; Enter opens a file or enters a folder, Bksp
 * goes up. Manage files with single keys: n new file, m new folder, r rename,
 * d delete, c copy, x cut, v paste, q quit. */
#include "explorer.h"
#include "vga.h"
#include "keyboard.h"
#include "ramfs.h"
#include "edit.h"
#include "build.h"
#include "mouse.h"
#include "ui.h"
#include "printf.h"
#include "string.h"
#include "types.h"

static char cur[FS_NAME_LEN] = "/home";
static char clip[FS_NAME_LEN];
static int  clip_cut;

static int (*g_opener)(const char *path);   /* set by the shell: launch apps/scripts on open */
void explorer_set_opener(int (*opener)(const char *path)) { g_opener = opener; }
static void (*g_to_terminal)(const char *path);   /* 't' key: send file to the terminal */
void explorer_set_terminal(void (*to_terminal)(const char *path)) { g_to_terminal = to_terminal; }

static const char *ex_base(const char *p) { const char *b = p; for (const char *q = p; *q; q++) if (*q == '/') b = q + 1; return b; }
static void ex_parent(const char *p, char *out) {
    int last = 0, n = (int)strlen(p);
    for (int i = 0; i < n; i++) if (p[i] == '/') last = i;
    if (last == 0) strcpy(out, "/"); else { memcpy(out, p, last); out[last] = 0; }
}
static void ex_join(const char *dir, const char *name, char *out) {
    int o = 0; for (const char *p = dir; *p; p++) out[o++] = *p;
    if (!(o == 1 && out[0] == '/')) out[o++] = '/';
    for (const char *p = name; *p && o < FS_NAME_LEN - 1; p++) out[o++] = *p;
    out[o] = 0;
}
static int dir_empty(const char *path) {
    for (int i = 0; i < FS_MAX_FILES; i++) {
        fs_file_t *f = fs_at(i);
        if (f && f->used) { char pd[FS_NAME_LEN]; ex_parent(f->name, pd); if (!strcmp(pd, path)) return 0; }
    }
    return 1;
}
static void ex_input(const char *prompt, char *buf, int max) {
    for (int x = 1; x <= 78; x++) vga_cell(x, 23, ' ', VGA_WHITE, VGA_BLACK);
    int px = 2; for (int i = 0; prompt[i]; i++) vga_cell(px++, 23, prompt[i], VGA_YELLOW, VGA_BLACK);
    int len = 0; buf[0] = 0; int sx = px; vga_setcursor(sx, 23);
    for (;;) {
        char c = keyboard_getc();
        if (c == '\n' || c == '\r') { buf[len] = 0; return; }
        else if (c == 27) { buf[0] = 0; return; }
        else if (c == '\b') { if (len) { len--; vga_cell(sx+len, 23, ' ', VGA_WHITE, VGA_BLACK); vga_setcursor(sx+len, 23); } }
        else if (c >= 32 && c < 127 && len < max-1) { buf[len++] = c; vga_cell(sx+len-1, 23, c, VGA_WHITE, VGA_BLACK); vga_setcursor(sx+len, 23); }
    }
}

/* a right-click dropdown at (x,y); returns the chosen action key, or 0 if cancelled */
static char context_menu(int x, int y) {
    static const char *lbl[] = { "Open", "To Terminal", "Rename", "Delete", "Copy", "Cut", "Paste", "New File", "New Folder" };
    static const char  act[] = { '\n',   't',           'r',      'd',      'c',    'x',   'v',     'n',        'm' };
    const int N = 9, w = 16;
    if (x + w > 78) x = 78 - w; if (x < 1) x = 1;
    if (y + N + 2 > 23) y = 23 - N - 2; if (y < 1) y = 1;
    int sel = 0, pbtn = 0;
    for (;;) {
        ui_box(x, y, w, N + 2, VGA_WHITE, VGA_BLUE);
        for (int i = 0; i < N; i++) {
            uint8_t fg = (i==sel)?VGA_BLACK:VGA_WHITE, bg = (i==sel)?VGA_LGREY:VGA_BLUE;
            for (int c = 1; c < w-1; c++) vga_cell(x+c, y+1+i, ' ', fg, bg);
            for (int k = 0; lbl[i][k]; k++) vga_cell(x+2+k, y+1+i, lbl[i][k], fg, bg);
        }
        vga_setcursor(0, 24);
        for (;;) {
            char c = keyboard_trygetc();
            if (c == 27 || c == 'q') return 0;
            else if (c == 0x10) { sel = (sel+N-1)%N; break; }
            else if (c == 0x0E) { sel = (sel+1)%N; break; }
            else if (c == '\n') return act[sel];
            if (mouse_present()) {
                int mx, my, b; mouse_get(&mx, &my, &b);
                int r = my - (y+1), idx = (r>=0 && r<N && mx>=x && mx<x+w) ? r : -1;
                if ((b & 2) && !(pbtn & 2)) { pbtn = b; }   /* ignore the release of the opening right-click */
                else if ((b & 1) && !(pbtn & 1)) { pbtn = b; return idx>=0 ? act[idx] : 0; }
                else pbtn = b;
                if (idx >= 0 && idx != sel) { sel = idx; break; }
            }
            __asm__ volatile("hlt");
        }
    }
}

void explorer_run(void) {
    static char names[96][FS_NAME_LEN], paths[96][FS_NAME_LEN]; static int isdir[96];
    int sel = 0, pbtn = 0;
    for (;;) {
        int n = 0, atroot = !strcmp(cur, "/");
        if (!atroot) { strcpy(names[n], ".."); ex_parent(cur, paths[n]); isdir[n] = 1; n++; }
        for (int pass = 0; pass < 2; pass++)
            for (int i = 0; i < FS_MAX_FILES && n < 96; i++) {
                fs_file_t *f = fs_at(i); if (!f || !f->used) continue;
                char pd[FS_NAME_LEN]; ex_parent(f->name, pd);
                if (strcmp(pd, cur)) continue;
                if (pass == 0 && !f->is_dir) continue;
                if (pass == 1 && f->is_dir) continue;
                strcpy(names[n], ex_base(f->name)); strcpy(paths[n], f->name); isdir[n] = f->is_dir; n++;
            }
        if (sel >= n) sel = n ? n - 1 : 0; if (sel < 0) sel = 0;

        vga_clear();
        vga_setcolor(VGA_YELLOW, VGA_BLACK); kprintf(" Files   %s", cur);
        if (clip[0]) { vga_setcolor(VGA_DGREY, VGA_BLACK); kprintf("    [%s %s]", clip_cut ? "cut" : "copied", ex_base(clip)); }
        vga_setcolor(VGA_LGREY, VGA_BLACK);
        for (int i = 0; i < n; i++) {
            int y = 3 + i; if (y > 22) break;
            uint8_t fg = (i==sel) ? VGA_BLACK : (isdir[i] ? VGA_LCYAN : VGA_LGREY);
            uint8_t bg = (i==sel) ? VGA_LGREY : VGA_BLACK;
            for (int x = 2; x <= 64; x++) vga_cell(x, y, ' ', fg, bg);
            vga_cell(3, y, (i==sel) ? '>' : ' ', fg, bg);
            int x = 5; for (int k = 0; names[i][k]; k++) vga_cell(x++, y, names[i][k], fg, bg);
            if (isdir[i] && strcmp(names[i], "..")) vga_cell(x, y, '/', fg, bg);
        }
        vga_statusbar(" Enter/click open   right-click menu   n new  d del  q quit");
        vga_setcursor(0, 24);

        char k = 0; int rcx = 0, rcy = 0, rclick = 0;
        for (;;) {
            char c = keyboard_trygetc();
            if (c) { k = c; break; }
            if (mouse_present()) {
                int mx, my, b; mouse_get(&mx, &my, &b);
                int row = my - 3, idx = (row >= 0 && my <= 22 && mx >= 2 && mx <= 64) ? row : -1;
                if (idx >= n) idx = -1;
                if ((b & 1) && !(pbtn & 1)) { pbtn = b; if (idx>=0) sel=idx; k = '\n'; break; }
                if ((b & 2) && !(pbtn & 2)) { pbtn = b; if (idx>=0) sel=idx; rclick=1; rcx=mx; rcy=my; break; }
                pbtn = b;
                if (idx >= 0 && idx != sel) { sel = idx; break; }  /* hover -> redraw */
            }
            __asm__ volatile("hlt");
        }
        if (rclick) { k = context_menu(rcx, rcy); if (!k) continue; }
        if (k == 0) continue;
        if (k == 'q' || k == 27) break;
        else if (k == 't') {   /* send the selected file to the terminal as the current file */
            if (n && strcmp(names[sel], "..") && !isdir[sel] && g_to_terminal) { g_to_terminal(paths[sel]); break; }
        }
        else if (k == 0x10) { if (sel > 0) sel--; }
        else if (k == 0x0E) { if (sel < n-1) sel++; }
        else if (k == '\b' || k == 0x02) { if (!atroot) { char pp[FS_NAME_LEN]; ex_parent(cur, pp); strcpy(cur, pp); sel = 0; } }
        else if (k == '\n' || k == 0x06) {
            if (!n) continue;
            if (!strcmp(names[sel], "..")) { strcpy(cur, paths[sel]); sel = 0; }
            else if (isdir[sel]) { strcpy(cur, paths[sel]); sel = 0; }
            else if (!(g_opener && g_opener(paths[sel]))) edit_run(paths[sel]);  /* app/script, else edit */
        }
        else if (k == 'n') { char nm[FS_NAME_LEN]; ex_input("New file: ", nm, FS_NAME_LEN); if (nm[0]) { char fp[FS_NAME_LEN]; ex_join(cur, nm, fp); fs_create(fp); build_run(fp); } }
        else if (k == 'm') { char nm[FS_NAME_LEN]; ex_input("New folder: ", nm, FS_NAME_LEN); if (nm[0]) { char fp[FS_NAME_LEN]; ex_join(cur, nm, fp); fs_mkdir(fp); } }
        else if (k == 'r') { if (n && strcmp(names[sel], "..")) { char nm[FS_NAME_LEN]; ex_input("Rename to: ", nm, FS_NAME_LEN); if (nm[0]) { char np[FS_NAME_LEN]; ex_join(cur, nm, np); fs_move(paths[sel], np); } } }
        else if (k == 'd') {
            if (n && strcmp(names[sel], "..")) {
                if (isdir[sel]) { if (dir_empty(paths[sel])) fs_delete(paths[sel]); }
                else fs_delete(paths[sel]);
                if (sel > 0) sel--;
            }
        }
        else if (k == 'c') { if (n && strcmp(names[sel], "..") && !isdir[sel]) { strcpy(clip, paths[sel]); clip_cut = 0; } }
        else if (k == 'x') { if (n && strcmp(names[sel], "..")) { strcpy(clip, paths[sel]); clip_cut = 1; } }
        else if (k == 'v' || k == 'p') {
            if (clip[0]) {
                char dst[FS_NAME_LEN]; ex_join(cur, ex_base(clip), dst);
                if (clip_cut) fs_move(clip, dst);
                else { fs_file_t *s = fs_find(clip); if (s && !s->is_dir) fs_write(dst, s->data, s->len); }
                clip[0] = 0;
            }
        }
    }
    vga_statusbar(" type 'menu' for Home    help  commands");
    vga_clear();
    vga_setcolor(VGA_LCYAN, VGA_BLACK); kputs("Files closed.\n");
    vga_setcolor(VGA_LGREY, VGA_BLACK);
}
