/* media.c - the Media Player. HisokaOS has a real 32-bpp framebuffer, so this plays
 * COLOR PIXEL media: an animated color field (real moving pixels) and BMP images /
 * slideshows (reusing the image decoder). No video codec exists (can't decode MP4),
 * so "playback" is honest: animated pixels and image frames, in full color. */
#include "media.h"
#include "gfx.h"
#include "vga.h"
#include "keyboard.h"
#include "printf.h"
#include "ramfs.h"
#include "imgview.h"
#include "ui.h"
#include "pit.h"
#include "string.h"
#include "types.h"

/* animated color field - real moving color pixels in the framebuffer */
static void color_demo(void) {
    if (!gfx_available()) {
        vga_clear(); vga_setcolor(VGA_LRED, VGA_BLACK);
        kputs("Media: no graphics adapter found.\n"); vga_setcolor(VGA_LGREY, VGA_BLACK);
        kputs("press any key...\n"); keyboard_getc(); return;
    }
    gfx_enter(1024, 768);
    uint32_t frame = 0;
    for (;;) {
        for (int by = 0; by < 96; by++)
            for (int bx = 0; bx < 128; bx++) {
                uint32_t r = (uint32_t)((bx*2 + frame) & 0xFF);
                uint32_t g = (uint32_t)((by*3 + frame/2) & 0xFF);
                uint32_t b = (uint32_t)(((bx ^ by)*4 + frame) & 0xFF);
                gfx_fill(bx*8, by*8, 8, 8, (r<<16)|(g<<8)|b);
            }
        frame += 6;
        if (keyboard_trygetc()) break;
    }
    gfx_exit();
}

/* collect image files in /home/Pictures */
static int list_pictures(char out[64][FS_NAME_LEN]) {
    int n = 0;
    for (int i = 0; i < FS_MAX_FILES && n < 64; i++) {
        fs_file_t *f = fs_at(i);
        if (!f || !f->used || f->is_dir) continue;
        const char *nm = f->name;
        if (strncmp(nm, "/home/Pictures/", 15)) continue;
        int deeper = 0; for (const char *p = nm + 15; *p; p++) if (*p == '/') deeper = 1;
        if (!deeper && imgview_is_image(nm)) { strcpy(out[n], nm); n++; }
    }
    return n;
}
static const char *pic_base(const char *p) { const char *b = p; for (const char *q = p; *q; q++) if (*q == '/') b = q + 1; return b; }

static void pictures_view(void) {
    static char pics[64][FS_NAME_LEN];
    int sel = 0;
    for (;;) {
        int n = list_pictures(pics);
        if (sel >= n) sel = n ? n - 1 : 0;
        vga_clear();
        ui_panel(2, 1, 76, 22, "Pictures", VGA_LCYAN, VGA_BLACK);
        if (!n) {
            ui_text(5, 4, "No images in /home/Pictures yet.", VGA_DGREY, VGA_BLACK);
            ui_text(5, 6, "Get one with:  download <url> /home/Pictures/pic.bmp", VGA_LGREY, VGA_BLACK);
            ui_text(5, 7, "(BMP images; the Chromium browser also produces BMP)", VGA_DGREY, VGA_BLACK);
        }
        for (int i = 0; i < n; i++) {
            int y = 3 + i; if (y > 20) break;
            uint8_t fg = (i==sel)?VGA_BLACK:VGA_LGREY, bg = (i==sel)?VGA_LGREY:VGA_BLACK;
            for (int x = 4; x <= 75; x++) vga_cell(x, y, ' ', fg, bg);
            int x = 5; for (const char *p = pic_base(pics[i]); *p && x < 75; p++) vga_cell(x++, y, *p, fg, bg);
        }
        vga_statusbar(" PICTURES  Up/Dn  Enter view  s slideshow  q back");
        vga_setcursor(0, 24);
        char k = keyboard_getc();
        if (k == 'q' || k == 27) return;
        else if (k == 0x10) { if (sel > 0) sel--; }
        else if (k == 0x0E) { if (sel < n-1) sel++; }
        else if (k == '\n') { if (n) imgview_run(pics[sel]); }
        else if (k == 's') {   /* slideshow: each image, ~2.5s, any key stops */
            for (int i = 0; i < n; i++) {
                imgview_run(pics[i]);   /* imgview waits for a key per image */
            }
        }
    }
}

void media_run(void) {
    static const char *items[] = { "Color Demo  - animated color pixels",
                                   "Pictures    - view images / slideshow",
                                   "About       - what this can play" };
    int sel = 0;
    for (;;) {
        vga_clear();
        ui_panel(2, 1, 76, 22, "Media Player", VGA_LCYAN, VGA_BLACK);
        ui_text(5, 3, "Color-pixel media in the graphics framebuffer.", VGA_DGREY, VGA_BLACK);
        for (int i = 0; i < 3; i++) ui_card(8, 5 + i*4, 58, items[i], i == sel, VGA_BLUE);
        vga_statusbar(" MEDIA   Up/Dn select   Enter open   q quit");
        vga_setcursor(0, 24);
        char k = keyboard_getc();
        if (k == 'q' || k == 27) break;
        else if (k == 0x10) sel = (sel + 2) % 3;
        else if (k == 0x0E) sel = (sel + 1) % 3;
        else if (k == '\n') {
            if (sel == 0) color_demo();
            else if (sel == 1) pictures_view();
            else {
                vga_clear(); ui_panel(2, 1, 76, 22, "Media Player - About", VGA_LCYAN, VGA_BLACK);
                ui_text(5, 4, "HisokaOS has a real 32-bpp framebuffer, so it plays color", VGA_LGREY, VGA_BLACK);
                ui_text(5, 5, "pixels directly: the animated Color Demo, and BMP images.", VGA_LGREY, VGA_BLACK);
                ui_text(5, 7, "It cannot decode compressed video (MP4/H.264) - that needs", VGA_DGREY, VGA_BLACK);
                ui_text(5, 8, "a codec the OS doesn't have yet. Honest about that.", VGA_DGREY, VGA_BLACK);
                vga_statusbar(" any key to go back");
                keyboard_getc();
            }
        }
    }
    vga_statusbar("help  commands  man       HisokaOS 0.2  i386");
    vga_clear();
    vga_setcolor(VGA_LCYAN, VGA_BLACK); kputs("Media Player closed.\n"); vga_setcolor(VGA_LGREY, VGA_BLACK);
}
