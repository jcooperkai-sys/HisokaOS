/* ui.c - the shared TUI toolkit. Boxes use CP437 line-drawing glyphs (drawn as raw
 * byte codes via vga_cell), so apps get clean framed panels instead of plain text. */
#include "ui.h"
#include "vga.h"
#include "keyboard.h"
#include "string.h"

/* CP437 single-line box pieces */
#define TL 0xDA
#define TR 0xBF
#define BL 0xC0
#define BR 0xD9
#define HZ 0xC4
#define VT 0xB3
/* double-line */
#define DTL 0xC9
#define DTR 0xBB
#define DBL 0xC8
#define DBR 0xBC
#define DHZ 0xCD
#define DVT 0xBA

void ui_fill(int x, int y, int w, int h, uint8_t bg) {
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++) vga_cell(x + i, y + j, ' ', VGA_LGREY, bg);
}
void ui_box(int x, int y, int w, int h, uint8_t fg, uint8_t bg) {
    vga_cell(x, y, (char)TL, fg, bg); vga_cell(x+w-1, y, (char)TR, fg, bg);
    vga_cell(x, y+h-1, (char)BL, fg, bg); vga_cell(x+w-1, y+h-1, (char)BR, fg, bg);
    for (int i = 1; i < w-1; i++) { vga_cell(x+i, y, (char)HZ, fg, bg); vga_cell(x+i, y+h-1, (char)HZ, fg, bg); }
    for (int j = 1; j < h-1; j++) { vga_cell(x, y+j, (char)VT, fg, bg); vga_cell(x+w-1, y+j, (char)VT, fg, bg); }
}
void ui_dbox(int x, int y, int w, int h, uint8_t fg, uint8_t bg) {
    vga_cell(x, y, (char)DTL, fg, bg); vga_cell(x+w-1, y, (char)DTR, fg, bg);
    vga_cell(x, y+h-1, (char)DBL, fg, bg); vga_cell(x+w-1, y+h-1, (char)DBR, fg, bg);
    for (int i = 1; i < w-1; i++) { vga_cell(x+i, y, (char)DHZ, fg, bg); vga_cell(x+i, y+h-1, (char)DHZ, fg, bg); }
    for (int j = 1; j < h-1; j++) { vga_cell(x, y+j, (char)DVT, fg, bg); vga_cell(x+w-1, y+j, (char)DVT, fg, bg); }
}
void ui_text(int x, int y, const char *s, uint8_t fg, uint8_t bg) {
    for (int i = 0; s[i]; i++) vga_cell(x + i, y, s[i], fg, bg);
}
void ui_textn(int x, int y, const char *s, int maxw, uint8_t fg, uint8_t bg) {
    for (int i = 0; s[i] && i < maxw; i++) vga_cell(x + i, y, s[i], fg, bg);
}
void ui_center(int y, int x0, int w, const char *s, uint8_t fg, uint8_t bg) {
    int len = (int)strlen(s); int x = x0 + (w - len) / 2; if (x < x0) x = x0;
    ui_textn(x, y, s, w, fg, bg);
}
void ui_panel(int x, int y, int w, int h, const char *title, uint8_t fg, uint8_t bg) {
    ui_fill(x+1, y+1, w-2, h-2, bg);
    ui_box(x, y, w, h, fg, bg);
    if (title && *title) {
        int len = (int)strlen(title);
        int tx = x + (w - len - 2) / 2;
        vga_cell(tx, y, ' ', fg, bg);
        ui_text(tx + 1, y, title, VGA_WHITE, bg);
        vga_cell(tx + 1 + len, y, ' ', fg, bg);
    }
}
/* a tile that looks like a clickable button; highlighted when selected */
void ui_card(int x, int y, int w, const char *label, int selected, uint8_t accent) {
    uint8_t bg = selected ? accent : VGA_BLACK;
    uint8_t fg = selected ? VGA_WHITE : VGA_LGREY;
    ui_box(x, y, w, 3, selected ? VGA_WHITE : VGA_DGREY, bg);
    ui_fill(x+1, y+1, w-2, 1, bg);
    ui_textn(x + 2, y + 1, label, w - 4, fg, bg);
    if (selected) vga_cell(x + w - 2, y + 1, (char)0x10, VGA_WHITE, bg);   /* > marker */
}
void ui_hint(const char *s) { vga_statusbar(s); }

/* scrollable text inside a centered box; starts at the top; up/down/pgup/pgdn, q quits */
void ui_scrollbox_view(const char *title, const char *text) {
    int bx = 6, by = 2, bw = 68, bh = 21;          /* the box */
    int ix = bx + 2, iy = by + 1, iw = bw - 4, ih = bh - 2;
    /* index line starts */
    static int starts[2048]; int nl = 0; starts[nl++] = 0;
    int len = (int)strlen(text);
    for (int i = 0; i < len && nl < 2047; i++) if (text[i] == '\n') starts[nl++] = i + 1;
    int top = 0;
    for (;;) {
        vga_clear();
        ui_panel(bx, by, bw, bh, title, VGA_LCYAN, VGA_BLACK);
        for (int r = 0; r < ih; r++) {
            int ln = top + r; if (ln >= nl) break;
            int s = starts[ln], e = (ln + 1 < nl) ? starts[ln+1] - 1 : len;
            for (int c = 0, i = s; i < e && c < iw; i++, c++) vga_cell(ix + c, iy + r, text[i], VGA_LGREY, VGA_BLACK);
        }
        /* scrollbar */
        if (nl > ih) {
            int sbh = ih * ih / nl; if (sbh < 1) sbh = 1;
            int sby = top * ih / nl;
            for (int j = 0; j < ih; j++) vga_cell(bx + bw - 2, iy + j, (char)0xB0, VGA_DGREY, VGA_BLACK);
            for (int j = 0; j < sbh && sby + j < ih; j++) vga_cell(bx + bw - 2, iy + sby + j, (char)0xDB, VGA_LCYAN, VGA_BLACK);
        }
        ui_hint(" Up/Down scroll   PgUp/PgDn page   q close");
        vga_setcursor(0, 24);
        char c = keyboard_getc();
        if (c == 'q' || c == 27) break;
        else if (c == 0x0E) { if (top + 1 < nl) top++; }                 /* down */
        else if (c == 0x10) { if (top > 0) top--; }                      /* up   */
        else if (c == 0x1A) { top += ih; if (top > nl-1) top = nl-1; if (top<0) top=0; } /* PgDn */
        else if (c == 0x19) { top -= ih; if (top < 0) top = 0; }         /* PgUp */
    }
    vga_clear();
}
