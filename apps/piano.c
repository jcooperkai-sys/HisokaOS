/* piano.c - a playable piano on the PC speaker. The home-row keys play notes; Space
 * plays a scale. Real tones come out of the speaker. */
#include "piano.h"
#include "vga.h"
#include "keyboard.h"
#include "printf.h"
#include "speaker.h"
#include "ui.h"
#include "types.h"

void piano_run(void) {
    static const char keys[] = "asdfghjk";
    static const char *names[] = { "C", "D", "E", "F", "G", "A", "B", "C+" };
    static const uint32_t freq[] = { 262, 294, 330, 349, 392, 440, 494, 523 };
    vga_clear();
    ui_panel(2, 1, 76, 22, "Piano", VGA_LCYAN, VGA_BLACK);
    ui_text(5, 3, "Press the keys to play.   Space = scale.   q = quit.", VGA_DGREY, VGA_BLACK);
    for (int i = 0; i < 8; i++) {
        int x = 8 + i*8;
        ui_box(x, 7, 7, 5, VGA_WHITE, VGA_BLACK);
        vga_cell(x+3, 9, keys[i], VGA_LCYAN, VGA_BLACK);
        ui_text(x+2, 10, names[i], VGA_LGREY, VGA_BLACK);
    }
    int playing = -1;
    for (;;) {
        if (playing >= 0) { int x = 8 + playing*8; for (int c=1;c<6;c++) for(int r=1;r<4;r++) vga_cell(x+c,7+r,' ',VGA_WHITE,VGA_BLACK); vga_cell(x+3,9,keys[playing],VGA_LCYAN,VGA_BLACK); ui_text(x+2,10,names[playing],VGA_LGREY,VGA_BLACK); playing=-1; }
        ui_text(5, 14, "Note:        ", VGA_LGREY, VGA_BLACK);
        vga_statusbar(" PIANO   a s d f g h j k = notes   space scale   q quit");
        vga_setcursor(0, 24);
        char c = keyboard_getc();
        if (c == 'q' || c == 27) break;
        if (c == ' ') { for (int i = 0; i < 8; i++) speaker_beep(freq[i], 180); continue; }
        for (int i = 0; keys[i]; i++) if (c == keys[i]) {
            int x = 8 + i*8;
            for (int cc=1;cc<6;cc++) for(int r=1;r<4;r++) vga_cell(x+cc,7+r,(char)0xB1,VGA_LGREEN,VGA_BLACK);
            ui_text(5, 14, "Note: ", VGA_LGREY, VGA_BLACK); ui_text(11, 14, names[i], VGA_LGREEN, VGA_BLACK);
            speaker_beep(freq[i], 250);
            playing = i;
            break;
        }
    }
    speaker_off();
    vga_clear();
    vga_setcolor(VGA_LCYAN, VGA_BLACK); kputs("Piano closed.\n"); vga_setcolor(VGA_LGREY, VGA_BLACK);
}
