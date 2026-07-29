/* edit.c - a minimal full-screen text editor for the in-memory filesystem.
 *
 * The file lives in one flat buffer; the cursor is an index into it. Text is laid
 * out into the shell's content window with simple wrapping. Arrow keys move the
 * cursor (they arrive from the keyboard driver as Ctrl-B/F/P/N control codes),
 * printable keys insert, Backspace deletes, Ctrl-S saves to ramfs, Ctrl-Q quits.
 * The view scrolls vertically to keep the cursor on screen. */
#include "edit.h"
#include "vga.h"
#include "keyboard.h"
#include "ramfs.h"
#include "printf.h"
#include "string.h"
#include "types.h"

#define EX0 1
#define EY0 1
#define WIDTH  78          /* content columns 1..78 */
#define HEIGHT 23          /* content rows    1..23 */
#define MAXBUF 8192

#define K_LEFT  0x02
#define K_RIGHT 0x06
#define K_UP    0x10
#define K_DOWN  0x0E
#define K_SAVE  0x13       /* Ctrl-S */
#define K_QUIT  0x11       /* Ctrl-Q */

static char buf[MAXBUF];
static int  len, cur;
static char fname[FS_NAME_LEN];

/* wrapped row containing buffer index pos */
static int row_of(int pos) {
    int row = 0, col = 0;
    for (int i = 0; i < pos && i < len; i++) {
        if (buf[i] == '\n') { row++; col = 0; }
        else { if (++col >= WIDTH) { row++; col = 0; } }
    }
    return row;
}

/* (row,col) of the cursor in wrapped coordinates */
static void cursor_rc(int *prow, int *pcol) {
    int row = 0, col = 0;
    for (int i = 0; i < cur; i++) {
        if (buf[i] == '\n') { row++; col = 0; }
        else { if (++col >= WIDTH) { row++; col = 0; } }
    }
    *prow = row; *pcol = col;
}

/* buffer index nearest to (target_row, target_col) */
static int index_at(int trow, int tcol) {
    int row = 0, col = 0;
    for (int i = 0; i <= len; i++) {
        if (row == trow && col >= tcol) return i;
        if (i == len) return i;
        if (buf[i] == '\n') { if (row == trow) return i; row++; col = 0; }
        else { if (++col >= WIDTH) { if (row == trow) return i; row++; col = 0; } }
    }
    return len;
}

static void insert(char c) {
    if (len >= MAXBUF) return;
    for (int i = len; i > cur; i--) buf[i] = buf[i-1];
    buf[cur++] = c; len++;
}
static void backspace(void) {
    if (cur == 0) return;
    for (int i = cur - 1; i < len - 1; i++) buf[i] = buf[i+1];
    len--; cur--;
}

static void render(int top) {
    vga_clear();
    int row = 0, col = 0, curx = EX0, cury = EY0;
    for (int i = 0; i <= len; i++) {
        if (i == cur) {
            int sr = row - top;
            if (sr >= 0 && sr < HEIGHT) { curx = EX0 + col; cury = EY0 + sr; }
        }
        if (i == len) break;
        char c = buf[i];
        if (c == '\n') { row++; col = 0; continue; }
        int sr = row - top;
        if (sr >= 0 && sr < HEIGHT) vga_cell(EX0 + col, EY0 + sr, c, VGA_LGREY, VGA_BLACK);
        if (++col >= WIDTH) { row++; col = 0; }
    }
    vga_setcursor(curx, cury);
}

void edit_run(const char *name) {
    int n = 0;
    if (name) while (name[n] && n < FS_NAME_LEN - 1) { fname[n] = name[n]; n++; }
    fname[n] = 0;
    if (!fname[0]) strcpy(fname, "untitled");

    fs_file_t *f = fs_find(fname);
    len = 0;
    if (f) for (size_t i = 0; i < f->len && len < MAXBUF; i++) buf[len++] = (char)f->data[i];
    cur = len;

    char sb[80]; int k = 0;
    const char *p = " EDIT  "; while (*p) sb[k++] = *p++;
    for (int i = 0; fname[i]; i++) sb[k++] = fname[i];
    const char *h = "    Ctrl-S save   Ctrl-Q quit   arrows move";
    for (int i = 0; h[i]; i++) sb[k++] = h[i];
    sb[k] = 0;
    vga_statusbar(sb);

    int top = 0, quit = 0;
    while (!quit) {
        int cr = row_of(cur);
        if (cr < top) top = cr;
        if (cr > top + HEIGHT - 1) top = cr - (HEIGHT - 1);
        render(top);

        char c = keyboard_getc();
        if      (c == K_QUIT)  quit = 1;
        else if (c == K_SAVE)  fs_write(fname, buf, (size_t)len);
        else if (c == K_LEFT)  { if (cur > 0)   cur--; }
        else if (c == K_RIGHT) { if (cur < len) cur++; }
        else if (c == K_UP)    { int r, cl; cursor_rc(&r, &cl); if (r > 0) cur = index_at(r - 1, cl); }
        else if (c == K_DOWN)  { int r, cl; cursor_rc(&r, &cl);             cur = index_at(r + 1, cl); }
        else if (c == '\b' || c == 0x7F) backspace();
        else if (c == '\n' || c == '\r') insert('\n');
        else if (c >= 32 && c < 127)     insert(c);
    }

    vga_statusbar("help  commands  cls       HisokaOS 0.2  i386");
    vga_clear();
    vga_setcolor(VGA_LCYAN, VGA_BLACK);
    kprintf("edit: closed %s (%u bytes) - 'ls' to see it, 'cat %s' to view\n",
            fname, (uint32_t)len, fname);
    vga_setcolor(VGA_LGREY, VGA_BLACK);
}
