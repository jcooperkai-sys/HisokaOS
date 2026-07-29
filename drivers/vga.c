/* vga.c - text console with a windowed TUI frame.
 *
 * The 80x25 screen is split into chrome and a content area:
 *
 *   row 0          title bar     (white on blue, centered title)
 *   rows 1..23     content       col 0 and col 79 are blue side borders;
 *                                 text lives in cols 1..78 and scrolls here
 *   row 24         status bar    (white on blue, hint text)
 *
 * Everything the kernel prints flows into the content window; the bars and
 * borders stay put. This gives HisokaOS the framed look of a real OS shell. */
#include "vga.h"
#include "string.h"
#include "ports.h"

#define VGA_MEM   ((uint16_t *)0xB8000)
#define COLS      80
#define ROWS      25
#define WIN_TOP   1            /* first content row (below title bar)   */
#define WIN_BOT   23           /* last content row (above status bar)   */
#define WIN_LEFT  1            /* first content col (right of border)   */
#define WIN_RIGHT 78           /* last content col (left of border)     */

static uint8_t CHROME = (uint8_t)(VGA_WHITE | (VGA_BLUE << 4));   /* bars + borders (themeable) */

static size_t  row = WIN_TOP, col = WIN_LEFT;
static uint8_t color;                              /* current content color */

static inline uint16_t cell(char c, uint8_t clr) {
    return (uint16_t)(uint8_t)c | ((uint16_t)clr << 8);
}
static inline void put_at(int x, int y, char c, uint8_t clr) {
    VGA_MEM[y * COLS + x] = cell(c, clr);
}

void vga_setcolor(uint8_t fg, uint8_t bg) { color = fg | (bg << 4); }

static void move_cursor(void) {
    uint16_t pos = row * COLS + col;
    outb(0x3D4, 14); outb(0x3D5, pos >> 8);
    outb(0x3D4, 15); outb(0x3D5, pos & 0xFF);
}

/* paint a full-width bar with text centered on it */
static void draw_bar(int y, const char *text) {
    for (int x = 0; x < COLS; x++) put_at(x, y, ' ', CHROME);
    int len = (int)strlen(text);
    int start = (COLS - len) / 2; if (start < 1) start = 1;
    for (int i = 0; i < len && start + i < COLS; i++) put_at(start + i, y, text[i], CHROME);
}

static char title_text[COLS]  = "HisokaOS 0.2";
static char status_text[COLS] = "type 'help' for commands";

static void draw_borders(void) {
    for (int y = WIN_TOP; y <= WIN_BOT; y++) {
        put_at(0, y, ' ', CHROME);
        put_at(COLS - 1, y, ' ', CHROME);
    }
}

void vga_titlebar(const char *s)  { strncpy(title_text,  s, COLS - 1);  title_text[COLS-1]=0;  draw_bar(0, title_text); }
void vga_statusbar(const char *s) { strncpy(status_text, s, COLS - 1);  status_text[COLS-1]=0; draw_bar(ROWS - 1, status_text); }

/* recolor the title/status bars and side borders (used by Settings) */
void vga_set_theme(uint8_t fg, uint8_t bg) {
    CHROME = (uint8_t)(fg | (bg << 4));
    draw_bar(0, title_text);
    draw_bar(ROWS - 1, status_text);
    draw_borders();
}

/* clear the content window and always redraw the side borders (the "shield" so no
 * page is left without its left/right barrier). */
void vga_clear(void) {
    for (int y = WIN_TOP; y <= WIN_BOT; y++)
        for (int x = WIN_LEFT; x <= WIN_RIGHT; x++) put_at(x, y, ' ', color);
    draw_borders();
    row = WIN_TOP; col = WIN_LEFT;
    move_cursor();
}

void vga_init(void) {
    vga_setcolor(VGA_LGREY, VGA_BLACK);
    for (int i = 0; i < COLS * ROWS; i++) VGA_MEM[i] = cell(' ', color);   /* wipe all */
    draw_bar(0, title_text);
    draw_bar(ROWS - 1, status_text);
    draw_borders();
    row = WIN_TOP; col = WIN_LEFT;
    move_cursor();
}

/* scroll the content window up by one line; chrome and borders are preserved */
static void scroll(void) {
    if (row <= WIN_BOT) return;
    for (int y = WIN_TOP; y < WIN_BOT; y++)
        for (int x = 0; x < COLS; x++) VGA_MEM[y * COLS + x] = VGA_MEM[(y + 1) * COLS + x];
    for (int x = WIN_LEFT; x <= WIN_RIGHT; x++) put_at(x, WIN_BOT, ' ', color);
    put_at(0, WIN_BOT, ' ', CHROME); put_at(COLS - 1, WIN_BOT, ' ', CHROME);  /* keep the shields */
    row = WIN_BOT;
}

void vga_putc(char c) {
    if (c == '\n')      { col = WIN_LEFT; row++; }
    else if (c == '\r') { col = WIN_LEFT; }
    else if (c == '\b') { if (col > WIN_LEFT) { col--; put_at(col, row, ' ', color); } }
    else if (c == '\t') { col += 4 - ((col - WIN_LEFT) & 3); if (col > WIN_RIGHT) { col = WIN_LEFT; row++; } }
    else {
        put_at(col, row, c, color);
        if (++col > WIN_RIGHT) { col = WIN_LEFT; row++; }
    }
    scroll();
    move_cursor();
}

void vga_write(const char *s) { while (*s) vga_putc(*s++); }

/* draw a single character at an absolute screen position with explicit colors -
 * the building block for games and full-screen TUIs. Does not move the cursor. */
void vga_cell(int x, int y, char c, uint8_t fg, uint8_t bg) {
    if (x < 0 || x >= COLS || y < 0 || y >= ROWS) return;
    put_at(x, y, c, (uint8_t)(fg | (bg << 4)));
}

/* park the hardware cursor at an absolute position (for the text editor) */
void vga_setcursor(int x, int y) {
    uint16_t pos = (uint16_t)(y * COLS + x);
    outb(0x3D4, 14); outb(0x3D5, (uint8_t)(pos >> 8));
    outb(0x3D4, 15); outb(0x3D5, (uint8_t)(pos & 0xFF));
}

/* report where the text cursor currently sits (for shell line editing) */
void vga_getcursor(int *x, int *y) { *x = (int)col; *y = (int)row; }
