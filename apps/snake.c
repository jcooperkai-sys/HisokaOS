/* snake.c - the classic Snake, drawn straight onto the VGA text console.
 *
 * Input is non-blocking (keyboard_trygetc) so the snake keeps moving while we wait
 * for a key; the PIT tick counter paces the frames. WASD steers, Q quits. Eat the
 * food (*) to grow; hitting a wall or yourself ends the game. */
#include "snake.h"
#include "vga.h"
#include "keyboard.h"
#include "pit.h"
#include "printf.h"
#include "types.h"

/* playfield border, in absolute screen coords (inside the shell's content area) */
#define BX0 2
#define BX1 77
#define BY0 2
#define BY1 22
#define SMAX 600          /* max snake length */
#define STEP 13           /* PIT ticks between moves (~130 ms at 100 Hz) - relaxed pace */

static int sx[SMAX], sy[SMAX];   /* body cells; index 0 is the head */
static int slen;
static int dx, dy;               /* current direction */
static int fx, fy;               /* food position */
static int score;

static uint32_t rng;
static uint32_t rnd(void) { rng = rng * 1103515245u + 12345u; return (rng >> 16) & 0x7FFF; }

static int on_snake(int x, int y) {
    for (int i = 0; i < slen; i++) if (sx[i] == x && sy[i] == y) return 1;
    return 0;
}

static void place_food(void) {
    do {
        fx = BX0 + 1 + (int)(rnd() % (BX1 - BX0 - 1));
        fy = BY0 + 1 + (int)(rnd() % (BY1 - BY0 - 1));
    } while (on_snake(fx, fy));
    vga_cell(fx, fy, '*', VGA_LRED, VGA_BLACK);
}

static void draw_frame(void) {
    vga_clear();
    vga_setcolor(VGA_YELLOW, VGA_BLACK);
    kprintf(" Snake   score: %u        W A S D to move,  Q to quit", (uint32_t)score);
    vga_setcolor(VGA_LGREY, VGA_BLACK);
    for (int x = BX0; x <= BX1; x++) { vga_cell(x, BY0, '-', VGA_DGREY, VGA_BLACK); vga_cell(x, BY1, '-', VGA_DGREY, VGA_BLACK); }
    for (int y = BY0; y <= BY1; y++) { vga_cell(BX0, y, '|', VGA_DGREY, VGA_BLACK); vga_cell(BX1, y, '|', VGA_DGREY, VGA_BLACK); }
    vga_cell(BX0, BY0, '+', VGA_DGREY, VGA_BLACK); vga_cell(BX1, BY0, '+', VGA_DGREY, VGA_BLACK);
    vga_cell(BX0, BY1, '+', VGA_DGREY, VGA_BLACK); vga_cell(BX1, BY1, '+', VGA_DGREY, VGA_BLACK);
}

static void set_dir(int ndx, int ndy) {
    if (ndx == -dx && ndy == -dy) return;   /* no instant 180 reverse */
    dx = ndx; dy = ndy;
}

void snake_run(void) {
    rng = pit_ticks() + 1;
    slen = 4; dx = 1; dy = 0; score = 0;
    int cx = (BX0 + BX1) / 2, cy = (BY0 + BY1) / 2;
    for (int i = 0; i < slen; i++) { sx[i] = cx - i; sy[i] = cy; }

    draw_frame();
    for (int i = 0; i < slen; i++)
        vga_cell(sx[i], sy[i], i == 0 ? '@' : 'o', i == 0 ? VGA_LGREEN : VGA_GREEN, VGA_BLACK);
    place_food();

    uint32_t last = pit_ticks();
    int alive = 1, quit = 0;

    while (alive && !quit) {
        char c = keyboard_trygetc();
        while (c) {
            if      (c == 'w' || c == 'W') set_dir(0, -1);
            else if (c == 's' || c == 'S') set_dir(0,  1);
            else if (c == 'a' || c == 'A') set_dir(-1, 0);
            else if (c == 'd' || c == 'D') set_dir( 1, 0);
            else if (c == 'q' || c == 'Q' || c == 27) quit = 1;
            c = keyboard_trygetc();
        }

        if (pit_ticks() - last < STEP) { __asm__ volatile("hlt"); continue; }
        last = pit_ticks();

        int nx = sx[0] + dx, ny = sy[0] + dy;

        if (nx <= BX0 || nx >= BX1 || ny <= BY0 || ny >= BY1) { alive = 0; break; }
        for (int i = 0; i < slen; i++) if (sx[i] == nx && sy[i] == ny) { alive = 0; break; }
        if (!alive) break;

        int grow = (nx == fx && ny == fy);
        if (grow && slen < SMAX) {
            score++;
            for (int i = slen; i > 0; i--) { sx[i] = sx[i-1]; sy[i] = sy[i-1]; }
            slen++;
        } else {
            vga_cell(sx[slen-1], sy[slen-1], ' ', VGA_BLACK, VGA_BLACK);   /* erase tail */
            for (int i = slen - 1; i > 0; i--) { sx[i] = sx[i-1]; sy[i] = sy[i-1]; }
        }
        sx[0] = nx; sy[0] = ny;

        vga_cell(sx[1], sy[1], 'o', VGA_GREEN, VGA_BLACK);                  /* old head -> body */
        vga_cell(nx, ny, '@', VGA_LGREEN, VGA_BLACK);                       /* new head */
        if (grow) place_food();

        vga_cell(17, 1, '0' + (score / 10) % 10, VGA_YELLOW, VGA_BLACK);
        vga_cell(18, 1, '0' + score % 10, VGA_YELLOW, VGA_BLACK);
    }

    vga_clear();
    vga_setcolor(VGA_LCYAN, VGA_BLACK);
    if (quit) kprintf("Snake: quit. final score %u\n", (uint32_t)score);
    else      kprintf("Snake: game over! final score %u\n", (uint32_t)score);
    vga_setcolor(VGA_LGREY, VGA_BLACK);
}
