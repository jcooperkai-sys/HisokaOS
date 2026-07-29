/* ttt.c - Tic-Tac-Toe. You are X; the computer is O and plays a full minimax
 * search, so it never loses (the best you can get is a draw). Press 1-9 to drop
 * your mark in that square, Q to quit. */
#include "ttt.h"
#include "vga.h"
#include "keyboard.h"
#include "printf.h"
#include "types.h"

#define TW 8     /* cell width  */
#define TH 3     /* cell height */
#define TX 28    /* grid origin column */
#define TY 6     /* grid origin row    */

static char bd[9];

/* 'X', 'O', 'D' (draw), or 0 if the game is still going */
static char winner(void) {
    static const int L[8][3] = {
        {0,1,2},{3,4,5},{6,7,8}, {0,3,6},{1,4,7},{2,5,8}, {0,4,8},{2,4,6}
    };
    for (int i = 0; i < 8; i++) {
        char a = bd[L[i][0]];
        if (a != ' ' && a == bd[L[i][1]] && a == bd[L[i][2]]) return a;
    }
    for (int i = 0; i < 9; i++) if (bd[i] == ' ') return 0;
    return 'D';
}

/* minimax: O maximizes, X minimizes */
static int minimax(int ai_turn) {
    char w = winner();
    if (w == 'O') return 10;
    if (w == 'X') return -10;
    if (w == 'D') return 0;
    int best = ai_turn ? -1000 : 1000;
    for (int i = 0; i < 9; i++)
        if (bd[i] == ' ') {
            bd[i] = ai_turn ? 'O' : 'X';
            int s = minimax(!ai_turn);
            bd[i] = ' ';
            if (ai_turn) { if (s > best) best = s; }
            else         { if (s < best) best = s; }
        }
    return best;
}

static void ai_move(void) {
    int bi = -1, best = -1000;
    for (int i = 0; i < 9; i++)
        if (bd[i] == ' ') {
            bd[i] = 'O';
            int s = minimax(0);
            bd[i] = ' ';
            if (s > best) { best = s; bi = i; }
        }
    if (bi >= 0) bd[bi] = 'O';
}

static void draw(const char *msg) {
    vga_clear();
    vga_setcolor(VGA_YELLOW, VGA_BLACK);
    kprintf(" Tic-Tac-Toe    you = X    press 1-9 to play,  Q to quit");
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 3; c++) {
            int idx = r * 3 + c, x = TX + c * TW, y = TY + r * TH;
            for (int i = 0; i <= TW; i++) { vga_cell(x+i, y, '-', VGA_DGREY, VGA_BLACK); vga_cell(x+i, y+TH, '-', VGA_DGREY, VGA_BLACK); }
            for (int i = 0; i <= TH; i++) { vga_cell(x, y+i, '|', VGA_DGREY, VGA_BLACK); vga_cell(x+TW, y+i, '|', VGA_DGREY, VGA_BLACK); }
            char ch     = (bd[idx] == ' ') ? (char)('1' + idx) : bd[idx];
            uint8_t col = (bd[idx] == 'X') ? VGA_LCYAN : (bd[idx] == 'O') ? VGA_LRED : VGA_DGREY;
            vga_cell(x + TW/2, y + 1, ch, col, VGA_BLACK);
        }
    if (msg) {
        vga_setcolor(VGA_LGREEN, VGA_BLACK);
        for (int k = 0; msg[k]; k++) vga_cell(TX + k, TY + 3*TH + 2, msg[k], VGA_LGREEN, VGA_BLACK);
    }
}

void ttt_run(void) {
    for (int i = 0; i < 9; i++) bd[i] = ' ';
    draw(0);
    int quit = 0;
    while (!quit) {
        char c = keyboard_getc();
        if (c == 'q' || c == 'Q' || c == 27) { quit = 1; break; }
        if (c < '1' || c > '9') continue;
        int idx = c - '1';
        if (bd[idx] != ' ') continue;
        bd[idx] = 'X';
        char w = winner();
        if (!w) { ai_move(); w = winner(); }
        if (w) {
            draw(w == 'X' ? "You win!" : w == 'O' ? "Computer wins." : "Draw.");
            keyboard_getc();
            break;
        }
        draw(0);
    }
    vga_clear();
    vga_setcolor(VGA_LCYAN, VGA_BLACK); kputs("Tic-Tac-Toe: thanks for playing.\n");
    vga_setcolor(VGA_LGREY, VGA_BLACK);
}
