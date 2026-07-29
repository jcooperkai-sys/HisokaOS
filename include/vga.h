/* vga.h - 80x25 VGA text-mode console at 0xB8000. */
#ifndef HISOKA_VGA_H
#define HISOKA_VGA_H
#include "types.h"

enum vga_color {
    VGA_BLACK = 0, VGA_BLUE, VGA_GREEN, VGA_CYAN, VGA_RED, VGA_MAGENTA,
    VGA_BROWN, VGA_LGREY, VGA_DGREY, VGA_LBLUE, VGA_LGREEN, VGA_LCYAN,
    VGA_LRED, VGA_LMAGENTA, VGA_YELLOW, VGA_WHITE,
};

void vga_init(void);
void vga_clear(void);
void vga_setcolor(uint8_t fg, uint8_t bg);
void vga_putc(char c);
void vga_write(const char *s);
void vga_titlebar(const char *s);    /* set the top title-bar text  */
void vga_statusbar(const char *s);   /* set the bottom status-bar text */
void vga_cell(int x, int y, char c, uint8_t fg, uint8_t bg);  /* draw one cell (for games/TUIs) */
void vga_setcursor(int x, int y);                            /* park the hardware cursor */
void vga_getcursor(int *x, int *y);                          /* read the text cursor position */
void vga_set_theme(uint8_t fg, uint8_t bg);                  /* recolor the title/status bars + borders */

#endif
