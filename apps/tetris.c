/* tetris.c - Tetris. Each tetromino is a 4x4 bitmap with 4 rotations. Pieces fall
 * on a timer; arrows move/rotate, space hard-drops. Full rows clear and score. */
#include "tetris.h"
#include "vga.h"
#include "keyboard.h"
#include "pit.h"
#include "printf.h"
#include "types.h"

#define FW 10
#define FH 18
#define OX 24      /* field origin column (each cell is 2 chars wide) */
#define OY 3       /* field origin row */

static const uint16_t PIECES[7][4] = {
    {0x0F00,0x2222,0x00F0,0x4444},  /* I */
    {0x6600,0x6600,0x6600,0x6600},  /* O */
    {0x4E00,0x4640,0x0E40,0x4C40},  /* T */
    {0x6C00,0x4620,0x06C0,0x8C40},  /* S */
    {0xC600,0x2640,0x0C60,0x4C80},  /* Z */
    {0x8E00,0x6440,0x0E20,0x44C0},  /* J */
    {0x2E00,0x4460,0x0E80,0xC440},  /* L */
};
static const uint8_t PCOL[7] = { VGA_LCYAN, VGA_YELLOW, VGA_LMAGENTA, VGA_LGREEN, VGA_LRED, VGA_LBLUE, VGA_BROWN };

static uint8_t  field[FH][FW];
static uint32_t rng;
static int piece, rot, px, py, score, lines;

static uint32_t rnd(void) { rng = rng * 1103515245u + 12345u + pit_ticks(); return rng >> 16; }

static int collide(int p, int r, int x, int y) {
    uint16_t m = PIECES[p][r];
    for (int i = 0; i < 16; i++)
        if (m & (0x8000 >> i)) {
            int cx = x + (i % 4), cy = y + (i / 4);
            if (cx < 0 || cx >= FW || cy >= FH) return 1;
            if (cy >= 0 && field[cy][cx]) return 1;
        }
    return 0;
}
static void merge(void) {
    uint16_t m = PIECES[piece][rot];
    for (int i = 0; i < 16; i++)
        if (m & (0x8000 >> i)) {
            int cx = px + (i % 4), cy = py + (i / 4);
            if (cy >= 0 && cy < FH && cx >= 0 && cx < FW) field[cy][cx] = (uint8_t)(piece + 1);
        }
}
static void clear_lines(void) {
    for (int y = FH - 1; y >= 0; y--) {
        int full = 1;
        for (int x = 0; x < FW; x++) if (!field[y][x]) { full = 0; break; }
        if (full) {
            for (int yy = y; yy > 0; yy--) for (int x = 0; x < FW; x++) field[yy][x] = field[yy-1][x];
            for (int x = 0; x < FW; x++) field[0][x] = 0;
            lines++; score += 100; y++;
        }
    }
}
static int spawn(void) { piece = rnd() % 7; rot = 0; px = 3; py = 0; return !collide(piece, rot, px, py); }

static void cell2(int fx, int fy, uint8_t col) {
    int sx = OX + fx * 2, sy = OY + fy;
    vga_cell(sx,     sy, ' ', VGA_WHITE, col);
    vga_cell(sx + 1, sy, ' ', VGA_WHITE, col);
}
static void put(int x, int y, const char *s, uint8_t fg) { for (int i = 0; s[i]; i++) vga_cell(x + i, y, s[i], fg, VGA_BLACK); }
static void put_num(int x, int y, uint32_t v, uint8_t fg) {
    char b[12], t[12]; int i = 0, n = 0;
    if (!v) t[n++] = '0'; while (v) { t[n++] = (char)('0' + v % 10); v /= 10; }
    while (n) b[i++] = t[--n]; b[i] = 0; put(x, y, b, fg);
}

static void draw(void) {
    for (int y = 0; y <= FH; y++) { vga_cell(OX - 1, OY + y, '|', VGA_DGREY, VGA_BLACK); vga_cell(OX + FW*2, OY + y, '|', VGA_DGREY, VGA_BLACK); }
    for (int x = -1; x <= FW*2; x++) vga_cell(OX + x, OY + FH, '-', VGA_DGREY, VGA_BLACK);
    for (int y = 0; y < FH; y++) for (int x = 0; x < FW; x++) cell2(x, y, field[y][x] ? PCOL[field[y][x]-1] : VGA_BLACK);
    uint16_t m = PIECES[piece][rot];
    for (int i = 0; i < 16; i++) if (m & (0x8000 >> i)) { int cy = py + i/4; if (cy >= 0) cell2(px + i%4, cy, PCOL[piece]); }
    int rx = OX + FW*2 + 4;
    put(rx, OY + 2, "Score", VGA_YELLOW);  put_num(rx, OY + 3, (uint32_t)score, VGA_WHITE);
    put(rx, OY + 5, "Lines", VGA_YELLOW);  put_num(rx, OY + 6, (uint32_t)lines, VGA_WHITE);
    vga_setcursor(0, 24);
}

void tetris_run(void) {
    rng = pit_ticks() + 1;
    for (int y = 0; y < FH; y++) for (int x = 0; x < FW; x++) field[y][x] = 0;
    score = 0; lines = 0;
    vga_clear();
    vga_setcolor(VGA_YELLOW, VGA_BLACK); kputs(" Tetris\n");
    vga_setcolor(VGA_DGREY, VGA_BLACK);  kputs("  left/right move, up rotate, down soft-drop, space hard-drop, q quit\n");
    vga_setcolor(VGA_LGREY, VGA_BLACK);
    vga_statusbar(" TETRIS    arrows move/rotate    space drop    q quit");
    spawn();

    uint32_t last = pit_ticks();
    int over = 0, quit = 0;
    while (!over && !quit) {
        char k = keyboard_trygetc();
        if (k == 'q' || k == 27) quit = 1;
        else if (k == 0x02) { if (!collide(piece, rot, px-1, py)) px--; }
        else if (k == 0x06) { if (!collide(piece, rot, px+1, py)) px++; }
        else if (k == 0x10) { int nr = (rot + 1) & 3; if (!collide(piece, nr, px, py)) rot = nr; }
        else if (k == 0x0E) { if (!collide(piece, rot, px, py+1)) py++; }
        else if (k == ' ')  { while (!collide(piece, rot, px, py+1)) py++; merge(); clear_lines(); if (!spawn()) over = 1; }

        uint32_t interval = 22 - (uint32_t)(lines / 2); if (interval < 4) interval = 4;
        if (pit_ticks() - last >= interval) {
            last = pit_ticks();
            if (!collide(piece, rot, px, py+1)) py++;
            else { merge(); clear_lines(); if (!spawn()) over = 1; }
        }
        draw();
        __asm__ volatile("hlt");
    }
    vga_statusbar(" type 'menu' for Home    help  commands");
    vga_clear();
    vga_setcolor(VGA_LCYAN, VGA_BLACK);
    kprintf("Tetris: %s   score %u   lines %u\n", quit ? "quit" : "game over", (uint32_t)score, (uint32_t)lines);
    vga_setcolor(VGA_LGREY, VGA_BLACK);
}
