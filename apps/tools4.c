/* tools4.c - finance + health + habit apps. Boxed, real, features-for-features.
 * Integer math throughout (the kernel has no FPU). */
#include "tools4.h"
#include "vga.h"
#include "keyboard.h"
#include "printf.h"
#include "ramfs.h"
#include "string.h"
#include "ui.h"
#include "types.h"

/* prompt for a number at (x,y); returns -1 on Esc, else the value (>=0) */
static long ask_num(int x, int y, char *echo, int max) {
    int len = 0; echo[0] = 0; vga_setcursor(x, y);
    for (;;) {
        char c = keyboard_getc();
        if (c=='\n'||c=='\r') { echo[len]=0; break; }
        else if (c==27) return -1;
        else if (c=='\b') { if (len) { len--; vga_cell(x+len,y,' ',VGA_WHITE,VGA_BLACK); vga_setcursor(x+len,y); } }
        else if (c>='0'&&c<='9'&&len<max-1) { echo[len++]=c; vga_cell(x+len-1,y,c,VGA_WHITE,VGA_BLACK); vga_setcursor(x+len,y); }
    }
    long v = 0; for (int i = 0; echo[i]; i++) v = v*10 + (echo[i]-'0');
    return v;
}
static void show_num(int x, int y, const char *label, long v, uint8_t fg) {
    char b[48]; int o = 0; for (const char*p=label;*p;p++) b[o++]=*p;
    long av = v<0?-v:v; char t[12]; int m=0; if(!av)t[m++]='0'; while(av){t[m++]=(char)('0'+av%10);av/=10;} if(v<0)b[o++]='-'; while(m)b[o++]=t[--m]; b[o]=0;
    ui_text(x, y, b, fg, VGA_BLACK);
}

/* ======================= FINANCE (interest) ======================= */
void finance_run(void) {
    for (;;) {
        vga_clear();
        ui_panel(2, 1, 76, 22, "Finance - Interest Calculator", VGA_LCYAN, VGA_BLACK);
        ui_text(5, 3, "Principal     :", VGA_LGREY, VGA_BLACK);
        ui_text(5, 4, "Annual rate % :", VGA_LGREY, VGA_BLACK);
        ui_text(5, 5, "Years         :", VGA_LGREY, VGA_BLACK);
        vga_statusbar(" FINANCE   enter numbers   Esc quit");
        char e[16];
        long P = ask_num(21, 3, e, 16); if (P < 0) { vga_clear(); return; }
        long rate = ask_num(21, 4, e, 16); if (rate < 0) { vga_clear(); return; }
        long yrs = ask_num(21, 5, e, 16); if (yrs < 0) { vga_clear(); return; }
        long si = P * rate * yrs / 100;
        long comp = P; for (long i = 0; i < yrs; i++) comp += comp * rate / 100;
        ui_text(5, 8, "Simple interest", VGA_DGREY, VGA_BLACK);
        show_num(5, 9,  "  interest : ", si, VGA_LGREEN);
        show_num(5, 10, "  total    : ", P + si, VGA_WHITE);
        ui_text(5, 12, "Compound (yearly)", VGA_DGREY, VGA_BLACK);
        show_num(5, 13, "  total    : ", comp, VGA_LGREEN);
        show_num(5, 14, "  interest : ", comp - P, VGA_WHITE);
        ui_text(5, 17, "any key = again,  q = quit", VGA_DGREY, VGA_BLACK);
        vga_setcursor(0, 24);
        char k = keyboard_getc(); if (k=='q'||k==27) { vga_clear(); return; }
    }
}

/* ======================= TIP CALCULATOR ======================= */
void tip_run(void) {
    for (;;) {
        vga_clear();
        ui_panel(2, 1, 76, 22, "Tip Calculator", VGA_LCYAN, VGA_BLACK);
        ui_text(5, 3, "Bill total :", VGA_LGREY, VGA_BLACK);
        ui_text(5, 4, "Tip %      :", VGA_LGREY, VGA_BLACK);
        ui_text(5, 5, "Split N    :", VGA_LGREY, VGA_BLACK);
        vga_statusbar(" TIP   enter numbers   Esc quit");
        char e[16];
        long bill = ask_num(18, 3, e, 16); if (bill < 0) { vga_clear(); return; }
        long tp = ask_num(18, 4, e, 16); if (tp < 0) { vga_clear(); return; }
        long n = ask_num(18, 5, e, 16); if (n < 0) { vga_clear(); return; } if (n == 0) n = 1;
        long tip = bill * tp / 100, total = bill + tip;
        show_num(5, 8,  "Tip       : ", tip, VGA_LGREEN);
        show_num(5, 9,  "Total     : ", total, VGA_WHITE);
        show_num(5, 10, "Per person: ", total / n, VGA_LCYAN);
        ui_text(5, 13, "any key = again,  q = quit", VGA_DGREY, VGA_BLACK);
        vga_setcursor(0, 24);
        char k = keyboard_getc(); if (k=='q'||k==27) { vga_clear(); return; }
    }
}

/* ======================= BMI ======================= */
void bmi_run(void) {
    for (;;) {
        vga_clear();
        ui_panel(2, 1, 76, 22, "BMI Calculator", VGA_LCYAN, VGA_BLACK);
        ui_text(5, 3, "Weight (kg) :", VGA_LGREY, VGA_BLACK);
        ui_text(5, 4, "Height (cm) :", VGA_LGREY, VGA_BLACK);
        vga_statusbar(" BMI   enter numbers   Esc quit");
        char e[16];
        long w = ask_num(19, 3, e, 16); if (w < 0) { vga_clear(); return; }
        long h = ask_num(19, 4, e, 16); if (h < 0) { vga_clear(); return; } if (h == 0) h = 1;
        long bmi10 = w * 100000 / (h * h);   /* BMI * 10 */
        show_num(5, 7, "BMI x10   : ", bmi10, VGA_LCYAN);
        const char *cat = bmi10 < 185 ? "underweight" : bmi10 < 250 ? "normal" : bmi10 < 300 ? "overweight" : "obese";
        ui_text(5, 8, "Category  : ", VGA_LGREY, VGA_BLACK); ui_text(17, 8, cat, VGA_LGREEN, VGA_BLACK);
        ui_text(5, 11, "any key = again,  q = quit", VGA_DGREY, VGA_BLACK);
        vga_setcursor(0, 24);
        char k = keyboard_getc(); if (k=='q'||k==27) { vga_clear(); return; }
    }
}

/* ======================= HABIT TRACKER ======================= */
#define H_MAX 40
#define H_LEN 60
static char h_name[H_MAX][H_LEN]; static int h_done[H_MAX]; static int h_streak[H_MAX]; static int h_n;
static int h_atoi(const char *s) { int v=0; while(*s>='0'&&*s<='9') v=v*10+(*s++-'0'); return v; }
static void habits_load(void) {
    h_n = 0; fs_file_t *f = fs_find("/home/habits.txt"); if (!f || f->is_dir) return;
    char line[H_LEN+16]; int li = 0;
    for (size_t k = 0; k <= f->len && h_n < H_MAX; k++) {
        char c = (k < f->len) ? (char)f->data[k] : '\n';
        if (c == '\n') {
            line[li] = 0;
            if (li > 0) {  /* format: done|streak|name */
                int p = 0; h_done[h_n] = line[0]-'0'; p = 2;
                char sb[8]; int s = 0; while (line[p] && line[p] != '|' && s < 7) sb[s++] = line[p++]; sb[s] = 0; h_streak[h_n] = h_atoi(sb); if (line[p]=='|') p++;
                strncpy(h_name[h_n], line+p, H_LEN-1); h_name[h_n][H_LEN-1]=0; h_n++;
            }
            li = 0;
        } else if (li < H_LEN+14) line[li++] = c;
    }
}
static void habits_save(void) {
    static char out[H_MAX*(H_LEN+10)]; int o = 0;
    for (int i = 0; i < h_n; i++) {
        out[o++] = (char)('0'+h_done[i]); out[o++] = '|';
        int v = h_streak[i]; char t[8]; int m=0; if(!v)t[m++]='0'; while(v){t[m++]=(char)('0'+v%10);v/=10;} while(m)out[o++]=t[--m];
        out[o++] = '|'; for (int k=0;h_name[i][k];k++) out[o++]=h_name[i][k]; out[o++]='\n';
    }
    fs_write("/home/habits.txt", out, (size_t)o);
}
void habits_run(void) {
    habits_load();
    int sel = 0;
    for (;;) {
        vga_clear();
        ui_panel(2, 1, 76, 22, "Habit Tracker", VGA_LCYAN, VGA_BLACK);
        ui_text(5, 3, "Done  Streak  Habit", VGA_DGREY, VGA_BLACK);
        if (!h_n) ui_text(6, 5, "No habits. Press 'a' to add one.", VGA_DGREY, VGA_BLACK);
        for (int i = 0; i < h_n; i++) {
            int y = 5 + i; if (y > 20) break;
            uint8_t fg=(i==sel)?VGA_BLACK:VGA_LGREY, bg=(i==sel)?VGA_LGREY:VGA_BLACK;
            for (int x=4;x<=75;x++) vga_cell(x,y,' ',fg,bg);
            vga_cell(6, y, h_done[i] ? 'x' : ' ', h_done[i]?VGA_LGREEN:fg, bg);
            vga_cell(5, y, '[', fg, bg); vga_cell(7, y, ']', fg, bg);
            { int v=h_streak[i]; char t[6]; int m=0; if(!v)t[m++]='0'; while(v){t[m++]=(char)('0'+v%10);v/=10;} int x=12; while(m) vga_cell(x++,y,t[--m],fg,bg); }
            int x = 20; for (int k=0;h_name[i][k]&&x<75;k++) vga_cell(x++, y, h_name[i][k], fg, bg);
        }
        vga_statusbar(" HABITS  Up/Dn  Space done(+streak)  a add  d del  q quit");
        vga_setcursor(0, 24);
        char k = keyboard_getc();
        if (k=='q'||k==27) { habits_save(); break; }
        else if (k==0x10) { if (sel>0) sel--; }
        else if (k==0x0E) { if (sel<h_n-1) sel++; }
        else if (k==' ') { if (h_n) { h_done[sel]=!h_done[sel]; if (h_done[sel]) h_streak[sel]++; else if (h_streak[sel]>0) h_streak[sel]--; habits_save(); } }
        else if (k=='a') { if (h_n<H_MAX) { for (int x=4;x<=75;x++) vga_cell(x,23,' ',VGA_WHITE,VGA_BLACK); ui_text(4,23,"Habit: ",VGA_YELLOW,VGA_BLACK); char b[H_LEN]; int len=0; b[0]=0; vga_setcursor(11,23); for(;;){ char c=keyboard_getc(); if(c=='\n'||c=='\r'){b[len]=0;break;} else if(c==27){b[0]=0;break;} else if(c=='\b'){if(len){len--;vga_cell(11+len,23,' ',VGA_WHITE,VGA_BLACK);vga_setcursor(11+len,23);}} else if(c>=32&&c<127&&len<H_LEN-1){b[len++]=c;vga_cell(11+len-1,23,c,VGA_WHITE,VGA_BLACK);vga_setcursor(11+len,23);} } if (b[0]) { strcpy(h_name[h_n],b); h_done[h_n]=0; h_streak[h_n]=0; sel=h_n; h_n++; habits_save(); } } }
        else if (k=='d') { if (h_n) { for (int i=sel;i<h_n-1;i++){strcpy(h_name[i],h_name[i+1]);h_done[i]=h_done[i+1];h_streak[i]=h_streak[i+1];} h_n--; if(sel>=h_n&&sel>0)sel--; habits_save(); } }
    }
    vga_clear(); vga_setcolor(VGA_LCYAN,VGA_BLACK); kputs("Habits saved.\n"); vga_setcolor(VGA_LGREY,VGA_BLACK);
}
