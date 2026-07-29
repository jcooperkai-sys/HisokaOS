/* g2048.c - the 2048 puzzle on the VGA text console.
 *
 * Turn-based, so input is the simple blocking read: each WASD slides every tile in
 * that direction, equal tiles that collide merge once and add to the score, then a
 * new 2 (occasionally a 4) appears in a random empty cell. The game ends when the
 * board is full with no merges possible. Q quits. */
#include "g2048.h"
#include "vga.h"
#include "keyboard.h"
#include "pit.h"
#include "printf.h"
#include "types.h"

#define GX 26   /* grid origin column   */
#define GY 4    /* grid origin row      */
#define CW 8    /* cell width  (chars)  */
#define CH 3    /* cell height (rows)   */

static int      board[4][4];
static int      score;
static uint32_t rng;
static uint32_t rnd(void) { rng = rng * 1103515245u + 12345u; return (rng >> 16) & 0x7FFF; }

static int add_tile(void) {
    int er[16], ec[16], n = 0;
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++)
            if (!board[r][c]) { er[n] = r; ec[n] = c; n++; }
    if (!n) return 0;
    int k = rnd() % n;
    board[er[k]][ec[k]] = (rnd() % 10 == 0) ? 4 : 2;   /* 10% fours */
    return 1;
}

/* slide/merge a 4-cell line toward index 0; returns 1 if anything changed */
static int slide_line(int *v) {
    int tmp[4] = {0,0,0,0}, t = 0, out[4] = {0,0,0,0}, o = 0, moved = 0;
    for (int i = 0; i < 4; i++) if (v[i]) tmp[t++] = v[i];
    for (int i = 0; i < 4; i++) {
        if (!tmp[i]) break;
        if (i + 1 < 4 && tmp[i] == tmp[i+1]) { out[o++] = tmp[i] * 2; score += tmp[i] * 2; i++; }
        else out[o++] = tmp[i];
    }
    for (int i = 0; i < 4; i++) { if (v[i] != out[i]) moved = 1; v[i] = out[i]; }
    return moved;
}

/* gather each line in the chosen direction, slide it, write it back */
static int move_dir(char d) {
    int moved = 0;
    for (int i = 0; i < 4; i++) {
        int line[4];
        for (int j = 0; j < 4; j++)
            line[j] = (d == 'a') ? board[i][j]   : (d == 'd') ? board[i][3-j]
                    : (d == 'w') ? board[j][i]   : board[3-j][i];
        if (slide_line(line)) moved = 1;
        for (int j = 0; j < 4; j++) {
            if      (d == 'a') board[i][j]   = line[j];
            else if (d == 'd') board[i][3-j] = line[j];
            else if (d == 'w') board[j][i]   = line[j];
            else               board[3-j][i] = line[j];
        }
    }
    return moved;
}

static int can_move(void) {
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++) {
            if (!board[r][c]) return 1;
            if (c + 1 < 4 && board[r][c] == board[r][c+1]) return 1;
            if (r + 1 < 4 && board[r][c] == board[r+1][c]) return 1;
        }
    return 0;
}

static uint8_t tile_color(int v) {
    switch (v) {
        case 2:   return VGA_LGREY;  case 4:   return VGA_WHITE;
        case 8:   return VGA_YELLOW; case 16:  return VGA_LRED;
        case 32:  return VGA_RED;    case 64:  return VGA_LMAGENTA;
        case 128: case 256:  return VGA_LCYAN;
        default:  return VGA_LGREEN;
    }
}

static void put_centered(int x, int y, int v, uint8_t col) {
    char b[6], tmp[6]; int n = v, t = 0, i = 0;
    while (n) { tmp[t++] = '0' + n % 10; n /= 10; }
    while (t) b[i++] = tmp[--t]; b[i] = 0;
    int sx = x + (CW - i) / 2;
    for (int k = 0; k < i; k++) vga_cell(sx + k, y, b[k], col, VGA_BLACK);
}

static void draw(void) {
    vga_clear();
    vga_setcolor(VGA_YELLOW, VGA_BLACK);
    kprintf(" 2048     score: %u        W A S D to slide,  Q to quit", (uint32_t)score);
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++) {
            int x = GX + c * CW, y = GY + r * CH;
            for (int i = 0; i <= CW; i++) { vga_cell(x+i, y, '-', VGA_DGREY, VGA_BLACK); vga_cell(x+i, y+CH, '-', VGA_DGREY, VGA_BLACK); }
            for (int i = 0; i <= CH; i++) { vga_cell(x, y+i, '|', VGA_DGREY, VGA_BLACK); vga_cell(x+CW, y+i, '|', VGA_DGREY, VGA_BLACK); }
            vga_cell(x, y, '+', VGA_DGREY, VGA_BLACK);    vga_cell(x+CW, y, '+', VGA_DGREY, VGA_BLACK);
            vga_cell(x, y+CH, '+', VGA_DGREY, VGA_BLACK); vga_cell(x+CW, y+CH, '+', VGA_DGREY, VGA_BLACK);
            if (board[r][c]) put_centered(x, y + 1, board[r][c], tile_color(board[r][c]));
        }
}

void g2048_run(void) {
    rng = pit_ticks() + 7;
    for (int r = 0; r < 4; r++) for (int c = 0; c < 4; c++) board[r][c] = 0;
    score = 0;
    add_tile(); add_tile(); draw();

    int quit = 0, over = 0;
    while (!quit && !over) {
        char c = keyboard_getc();
        int moved = 0;
        if      (c == 'q' || c == 'Q' || c == 27) quit = 1;
        else if (c == 'a' || c == 'A') moved = move_dir('a');
        else if (c == 'd' || c == 'D') moved = move_dir('d');
        else if (c == 'w' || c == 'W') moved = move_dir('w');
        else if (c == 's' || c == 'S') moved = move_dir('s');
        if (moved) { add_tile(); draw(); if (!can_move()) over = 1; }
    }

    vga_clear();
    vga_setcolor(VGA_LCYAN, VGA_BLACK);
    kprintf("2048: %s  final score %u\n", quit ? "quit" : "game over", (uint32_t)score);
    vga_setcolor(VGA_LGREY, VGA_BLACK);
}
