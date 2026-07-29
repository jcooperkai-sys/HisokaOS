/* clock.c - a big-digit clock. Reads the RTC and redraws HH:MM:SS in a 3x5 block
 * font, updating ~twice a second. Press any key to exit. */
#include "clock.h"
#include "vga.h"
#include "keyboard.h"
#include "pit.h"
#include "rtc.h"
#include "printf.h"
#include "ui.h"
#include "types.h"

/* 3 wide x 5 tall font for digits 0-9 */
static const char *FONT[10][5] = {
    {"###","# #","# #","# #","###"},
    {"  #","  #","  #","  #","  #"},
    {"###","  #","###","#  ","###"},
    {"###","  #","###","  #","###"},
    {"# #","# #","###","  #","  #"},
    {"###","#  ","###","  #","###"},
    {"###","#  ","###","# #","###"},
    {"###","  #","  #","  #","  #"},
    {"###","# #","###","# #","###"},
    {"###","# #","###","  #","###"},
};

static void glyph(int x, int y, const char *rows[5], uint8_t col) {
    for (int r = 0; r < 5; r++)
        for (int c = 0; c < 3; c++)
            vga_cell(x + c, y + r, rows[r][c] == '#' ? rows[r][c] : ' ',
                     rows[r][c] == '#' ? col : VGA_BLACK, VGA_BLACK);
}
static void colon(int x, int y, uint8_t col) {
    vga_cell(x, y + 1, '#', col, VGA_BLACK);
    vga_cell(x, y + 3, '#', col, VGA_BLACK);
}

void clock_run(void) {
    vga_clear();
    ui_panel(2, 1, 76, 22, "Clock", VGA_LCYAN, VGA_BLACK);
    vga_statusbar(" CLOCK    HH:MM:SS from the real-time clock    any key to exit");

    for (;;) {
        rtc_time_t t; rtc_now(&t);
        int d[6] = { t.hour/10, t.hour%10, t.min/10, t.min%10, t.sec/10, t.sec%10 };
        int x = 26, y = 7;                 /* layout: HH : MM : SS, centered in the box */
        glyph(x, y, FONT[d[0]], VGA_LGREEN); x += 4;
        glyph(x, y, FONT[d[1]], VGA_LGREEN); x += 4;
        colon(x, y, VGA_GREEN);              x += 2;
        glyph(x, y, FONT[d[2]], VGA_LGREEN); x += 4;
        glyph(x, y, FONT[d[3]], VGA_LGREEN); x += 4;
        colon(x, y, VGA_GREEN);              x += 2;
        glyph(x, y, FONT[d[4]], VGA_LGREEN); x += 4;
        glyph(x, y, FONT[d[5]], VGA_LGREEN);
        char db[12];
        db[0]=(char)('0'+(t.year/1000)%10); db[1]=(char)('0'+(t.year/100)%10); db[2]=(char)('0'+(t.year/10)%10); db[3]=(char)('0'+t.year%10);
        db[4]='-'; db[5]=(char)('0'+t.month/10); db[6]=(char)('0'+t.month%10); db[7]='-'; db[8]=(char)('0'+t.day/10); db[9]=(char)('0'+t.day%10); db[10]=0;
        ui_center(14, 2, 76, db, VGA_DGREY, VGA_BLACK);
        vga_setcursor(0, 24);

        uint32_t deadline = pit_ticks() + 50;   /* ~0.5 s */
        char k = 0;
        while (pit_ticks() < deadline) { k = keyboard_trygetc(); if (k) break; __asm__ volatile("hlt"); }
        if (k) break;
    }
    vga_statusbar("help  commands  man       HisokaOS 0.2  i386");
    vga_clear();
    vga_setcolor(VGA_LCYAN, VGA_BLACK); kputs("Clock closed.\n");
    vga_setcolor(VGA_LGREY, VGA_BLACK);
}
