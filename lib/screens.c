/* screens.c - the system screens a real OS shows: a boot splash, shutdown/restart
 * screens, and a kernel-panic screen that counts down and reboots. */
#include "screens.h"
#include "vga.h"
#include "pit.h"
#include "keyboard.h"
#include "ports.h"
#include "string.h"

static const char *LOGO[5] = {
    "    __  ___                __         ____  _____",
    "   / / / (_)________  ____/ /_____ _ / __ \\/ ___/",
    "  / /_/ / / ___/ __ \\/ __  / __/ // // / / /\\__ \\ ",
    " / __  / (__  ) /_/ / /_/ / /_/ ,< // /_/ /___/ / ",
    "/_/ /_/_/____/\\____/\\__,_/\\__/_/|_| \\____//____/  ",
};

/* park the blinking text cursor offscreen so it doesn't sit on top of a full-screen splash */
static void hide_cursor(void) { vga_setcursor(0, 25); }

static void center(int row, const char *s, uint8_t fg, uint8_t bg) {
    int x = (80 - (int)strlen(s)) / 2; if (x < 0) x = 0;
    for (int i = 0; s[i]; i++) vga_cell(x + i, row, s[i], fg, bg);
}
static void draw_logo(int top, uint8_t fg) {
    for (int r = 0; r < 5; r++) center(top + r, LOGO[r], fg, VGA_BLACK);
}
static void reboot_now(void)   { uint8_t g = 0x02; while (g & 0x02) g = inb(0x64); outb(0x64, 0xFE); for (;;) __asm__ volatile("hlt"); }
static void poweroff_now(void) { outw(0x604, 0x2000); outw(0xB004, 0x2000); for (;;) __asm__ volatile("hlt"); }

static void spinner(uint32_t ticks, int sx, int sy) {
    const char *sp = "|/-\\";
    uint32_t start = pit_ticks(); int i = 0;
    while (pit_ticks() - start < ticks) {
        vga_cell(sx, sy, sp[i & 3], VGA_LGREEN, VGA_BLACK); i++;
        uint32_t t = pit_ticks(); while (pit_ticks() - t < 9) __asm__ volatile("hlt");
    }
}

/* The static boot screen - safe to draw before the timer is running (no spinner). */
void splash_draw(void) {
    vga_clear();
    hide_cursor();
    draw_logo(6, VGA_LGREEN);
    center(13, "version 0.2", VGA_DGREY, VGA_BLACK);
    center(16, "starting up", VGA_LGREY, VGA_BLACK);
}

/* Animate the boot spinner for `ticks` PIT ticks - needs interrupts/timer live. */
void splash_spin(uint32_t ticks) { spinner(ticks, 39, 18); }

void splash_boot(void) { splash_draw(); splash_spin(130); }

/* Shown on boot when a queued update is being applied. Real OS update screens warn
 * not to power off and show progress; here the work is committing the staged files. */
void splash_update(void) {
    vga_clear();
    hide_cursor();
    draw_logo(4, VGA_LGREEN);
    center(11, "Installing system update", VGA_WHITE, VGA_BLACK);
    center(12, "Do not turn off your computer", VGA_LRED, VGA_BLACK);
    int bw = 44, bx = (80 - bw) / 2, by = 15;
    for (int i = 0; i <= bw; i++) {
        for (int k = 0; k < bw; k++)
            vga_cell(bx + k, by, ' ', VGA_WHITE, k < i ? VGA_LGREEN : VGA_DGREY);
        int pct = i * 100 / bw;
        char p[5]; int n = 0;
        if (pct >= 100) { p[n++] = '1'; p[n++] = '0'; p[n++] = '0'; }
        else { if (pct >= 10) p[n++] = (char)('0' + pct / 10); p[n++] = (char)('0' + pct % 10); }
        p[n++] = '%'; p[n] = 0;
        center(17, p, VGA_LGREY, VGA_BLACK);
        uint32_t t = pit_ticks(); while (pit_ticks() - t < 5) __asm__ volatile("hlt");
    }
    center(19, "Update complete", VGA_LGREEN, VGA_BLACK);
    spinner(70, 39, 21);
}

void screen_message(const char *msg, int action) {
    vga_clear();
    hide_cursor();
    draw_logo(6, VGA_LGREEN);
    center(14, msg, VGA_WHITE, VGA_BLACK);
    spinner(action == 0 ? 80 : 140, 39, 17);
    if      (action == 1) reboot_now();
    else if (action == 2) poweroff_now();
}

void panic_sim(void) {
    hide_cursor();
    for (int y = 0; y < 25; y++) for (int x = 0; x < 80; x++) vga_cell(x, y, ' ', VGA_WHITE, VGA_RED);
    center(6,  "*** KERNEL PANIC ***", VGA_YELLOW, VGA_RED);
    center(9,  "A fatal error occurred and the system would restart.", VGA_WHITE, VGA_RED);
    center(11, "fault: simulated panic (preview)", VGA_WHITE, VGA_RED);
    center(14, "[ preview only - press any key to return ]", VGA_LGREY, VGA_RED);
    keyboard_getc();
}

void kernel_panic(const char *msg) {
    __asm__ volatile("cli");
    hide_cursor();
    for (int y = 0; y < 25; y++) for (int x = 0; x < 80; x++) vga_cell(x, y, ' ', VGA_WHITE, VGA_RED);
    center(6,  "*** KERNEL PANIC ***", VGA_YELLOW, VGA_RED);
    center(9,  "The system encountered a fatal error and must restart.", VGA_WHITE, VGA_RED);
    center(11, msg, VGA_WHITE, VGA_RED);
    for (int s = 5; s >= 1; s--) {
        char line[28]; int n = 0;
        const char *a = "restarting in "; while (*a) line[n++] = *a++;
        line[n++] = (char)('0' + s);
        const char *b = " ..."; while (*b) line[n++] = *b++;
        line[n] = 0;
        center(14, line, VGA_LGREY, VGA_RED);
        for (volatile uint32_t i = 0; i < 180000000u; i++) __asm__ volatile("nop");  /* ~1s busy-wait */
    }
    reboot_now();
}
