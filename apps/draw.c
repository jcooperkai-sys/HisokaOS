/* draw.c - a tiny paint program. Move the cursor with the arrow keys; press any
 * printable key to stamp that character in the current color; 1-7 pick a color;
 * Space erases the cell; Backspace clears the canvas; Ctrl-S saves the drawing to
 * a file (in /home/Pictures by default); q quits.
 *
 * Strokes are mirrored into a backing canvas buffer so the drawing can be saved. */
#include "draw.h"
#include "vga.h"
#include "keyboard.h"
#include "printf.h"
#include "ramfs.h"
#include "string.h"
#include "types.h"

#define K_SAVE 0x13            /* Ctrl-S */

static const uint8_t PAL[7] = { VGA_WHITE, VGA_LRED, VGA_YELLOW, VGA_LGREEN, VGA_LCYAN, VGA_LBLUE, VGA_LMAGENTA };
static char canvas[24][80];    /* screen rows 2..23, cols 1..78 are the canvas */

static void canvas_clear(void) { for (int y = 0; y < 24; y++) for (int x = 0; x < 80; x++) canvas[y][x] = ' '; }

static void header(int ci) {
    vga_setcolor(VGA_YELLOW, VGA_BLACK);
    kputs(" Draw  ");
    vga_setcolor(VGA_DGREY, VGA_BLACK);
    kprintf("color %u   arrows move, type to paint, 1-7 color, space erase, Ctrl-S save, bksp clear, q quit", (uint32_t)(ci + 1));
    vga_setcolor(VGA_LGREY, VGA_BLACK);
}

/* line-23 prompt; returns 0 if cancelled */
static int prompt_row(const char *label, char *buf, int max) {
    for (int x = 1; x <= 78; x++) vga_cell(x, 23, ' ', VGA_WHITE, VGA_BLACK);
    int px = 2; for (int i = 0; label[i]; i++) vga_cell(px++, 23, label[i], VGA_YELLOW, VGA_BLACK);
    int len = 0; buf[0] = 0; int sx = px; vga_setcursor(sx, 23);
    for (;;) {
        char c = keyboard_getc();
        if (c == '\n' || c == '\r') { buf[len] = 0; return len > 0; }
        else if (c == 27) { buf[0] = 0; return 0; }
        else if (c == '\b') { if (len) { len--; vga_cell(sx+len, 23, ' ', VGA_WHITE, VGA_BLACK); vga_setcursor(sx+len, 23); } }
        else if (c >= 32 && c < 127 && len < max-1) { buf[len++] = c; vga_cell(sx+len-1, 23, c, VGA_WHITE, VGA_BLACK); vga_setcursor(sx+len, 23); }
    }
}

static void status_row(const char *msg, uint8_t fg) {
    for (int x = 1; x <= 78; x++) vga_cell(x, 23, ' ', VGA_WHITE, VGA_BLACK);
    int x = 2; for (int i = 0; msg[i]; i++) vga_cell(x++, 23, msg[i], fg, VGA_BLACK);
}

static void save_canvas(void) {
    char nm[FS_NAME_LEN];
    if (!prompt_row("Save as (in /home/Pictures): ", nm, FS_NAME_LEN)) return;

    char path[FS_NAME_LEN]; int o = 0;
    if (nm[0] == '/') strcpy(path, nm);
    else {
        for (const char *p = "/home/Pictures/"; *p; p++) path[o++] = *p;
        for (int i = 0; nm[i] && o < FS_NAME_LEN - 1; i++) path[o++] = nm[i];
        path[o] = 0;
    }
    static char buf[24 * 80]; int b = 0;
    for (int y = 2; y <= 23; y++) {
        int last = 0; for (int x = 1; x <= 78; x++) if (canvas[y][x] != ' ') last = x;
        for (int x = 1; x <= last; x++) buf[b++] = canvas[y][x];
        buf[b++] = '\n';
    }
    fs_mkdir("/home/Pictures");
    int r = fs_write(path, buf, (size_t)b);
    if (r == 0) { char m[80]; int n = 0; for (const char *p = "saved to "; *p; p++) m[n++] = *p;
                  for (int i = 0; path[i] && n < 78; i++) m[n++] = path[i]; m[n] = 0; status_row(m, VGA_LGREEN); }
    else status_row("save failed (filesystem full?)", VGA_LRED);
}

void draw_run(void) {
    int cx = 39, cy = 12, ci = 0, quit = 0;
    canvas_clear();
    vga_clear();
    header(ci);
    vga_statusbar(" DRAW  arrows move  type paint  1-7 color  space erase  Ctrl-S save  q quit");
    vga_setcursor(cx, cy);

    while (!quit) {
        char k = keyboard_getc();
        if      (k == 'q' || k == 27)          quit = 1;
        else if (k == 0x10) { if (cy > 2)  cy--; }      /* up    */
        else if (k == 0x0E) { if (cy < 23) cy++; }      /* down  */
        else if (k == 0x02) { if (cx > 1)  cx--; }      /* left  */
        else if (k == 0x06) { if (cx < 78) cx++; }      /* right */
        else if (k >= '1' && k <= '7') ci = k - '1';
        else if (k == ' ')  { vga_cell(cx, cy, ' ', VGA_WHITE, VGA_BLACK); canvas[cy][cx] = ' '; }
        else if (k == K_SAVE) save_canvas();
        else if (k == '\b') { vga_clear(); header(ci); canvas_clear(); }
        else if (k >= 33 && k < 127) { vga_cell(cx, cy, k, PAL[ci], VGA_BLACK); canvas[cy][cx] = k; }
        vga_setcursor(cx, cy);
    }
    vga_statusbar("help  commands  man       HisokaOS 0.2  i386");
    vga_clear();
    vga_setcolor(VGA_LCYAN, VGA_BLACK); kputs("Draw closed.\n");
    vga_setcolor(VGA_LGREY, VGA_BLACK);
}
