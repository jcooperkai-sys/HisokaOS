/* tools6.c - more real boxed apps (no games): percentage, text stats, number
 * spelling, pomodoro, times table. Integer math, features-for-features. */
#include "tools6.h"
#include "vga.h"
#include "keyboard.h"
#include "printf.h"
#include "string.h"
#include "pit.h"
#include "ui.h"
#include "types.h"

static int t6_text(int x, int y, char *b, int max) {
    int len = 0; b[0] = 0; vga_setcursor(x, y);
    for (;;) { char c = keyboard_getc();
        if (c=='\n'||c=='\r') { b[len]=0; return len; }
        else if (c==27) { b[0]=0; return -1; }
        else if (c=='\b') { if (len) { len--; vga_cell(x+len,y,' ',VGA_WHITE,VGA_BLACK); vga_setcursor(x+len,y); } }
        else if (c>=32&&c<127&&len<max-1&&x+len<76) { b[len++]=c; vga_cell(x+len-1,y,c,VGA_WHITE,VGA_BLACK); vga_setcursor(x+len,y); } }
}
static long t6_num(int x, int y) { char b[16]; if (t6_text(x,y,b,16)<0) return -1; long v=0; for (int i=0;b[i];i++) if(b[i]>='0'&&b[i]<='9') v=v*10+(b[i]-'0'); return v; }
static int t6_again(void) { char k=keyboard_getc(); return (k=='q'||k==27); }
static void show_n(int x, int y, const char *label, long v, uint8_t fg) {
    char b[48]; int o=0; for (const char*p=label;*p;p++) b[o++]=*p;
    long av=v<0?-v:v; char t[12]; int m=0; if(!av)t[m++]='0'; while(av){t[m++]=(char)('0'+av%10);av/=10;} if(v<0)b[o++]='-'; while(m)b[o++]=t[--m]; b[o]=0;
    ui_text(x,y,b,fg,VGA_BLACK);
}

/* ======================= PERCENTAGE ======================= */
void percent_run(void) {
    for (;;) {
        vga_clear();
        ui_panel(2,1,76,22,"Percentage Calculator",VGA_LCYAN,VGA_BLACK);
        ui_text(5,3,"A :",VGA_LGREY,VGA_BLACK); long a=t6_num(9,3); if(a<0){vga_clear();return;}
        ui_text(5,4,"B :",VGA_LGREY,VGA_BLACK); long b=t6_num(9,4); if(b<0){vga_clear();return;}
        show_n(5,7,  "A% of B        : ", a*b/100, VGA_LGREEN);
        show_n(5,8,  "A is __% of B  : ", b? a*100/b : 0, VGA_LCYAN);
        show_n(5,9,  "change A->B (%): ", a? (b-a)*100/a : 0, VGA_WHITE);
        ui_text(5,12,"any key = again,  q = quit",VGA_DGREY,VGA_BLACK);
        vga_statusbar(" PERCENT   enter A and B   Esc quit"); vga_setcursor(0,24);
        if (t6_again()) { vga_clear(); return; }
    }
}

/* ======================= TEXT STATS ======================= */
void textstats_run(void) {
    for (;;) {
        vga_clear();
        ui_panel(2,1,76,22,"Text Stats",VGA_LCYAN,VGA_BLACK);
        ui_text(5,3,"Text:",VGA_LGREY,VGA_BLACK);
        char t[128]; if (t6_text(11,3,t,128)<0) { vga_clear(); return; }
        int chars=0, words=0, vowels=0, inword=0;
        for (int i=0; t[i]; i++) {
            chars++;
            char c=t[i];
            if (c==' '||c=='\t') inword=0; else { if (!inword) words++; inword=1; }
            char lc = (c>='A'&&c<='Z')?(char)(c+32):c;
            if (lc=='a'||lc=='e'||lc=='i'||lc=='o'||lc=='u') vowels++;
        }
        show_n(5,6,"Characters : ", chars, VGA_LCYAN);
        show_n(5,7,"Words      : ", words, VGA_LCYAN);
        show_n(5,8,"Vowels     : ", vowels, VGA_LCYAN);
        show_n(5,9,"Consonants : ", chars-vowels, VGA_LCYAN);
        ui_text(5,12,"any key = again,  q = quit",VGA_DGREY,VGA_BLACK);
        vga_statusbar(" TEXT STATS   enter text   Esc quit"); vga_setcursor(0,24);
        if (t6_again()) { vga_clear(); return; }
    }
}

/* ======================= NUMBER TO WORDS ======================= */
static const char *ONES[] = {"zero","one","two","three","four","five","six","seven","eight","nine","ten",
    "eleven","twelve","thirteen","fourteen","fifteen","sixteen","seventeen","eighteen","nineteen"};
static const char *TENS[] = {"","","twenty","thirty","forty","fifty","sixty","seventy","eighty","ninety"};
static int spell_below_100(int n, char *o, int oi) {
    if (n < 20) { for (const char*p=ONES[n];*p;p++) o[oi++]=*p; return oi; }
    for (const char*p=TENS[n/10];*p;p++) o[oi++]=*p;
    if (n%10) { o[oi++]='-'; for (const char*p=ONES[n%10];*p;p++) o[oi++]=*p; }
    return oi;
}
void num2words_run(void) {
    for (;;) {
        vga_clear();
        ui_panel(2,1,76,22,"Number to Words",VGA_LCYAN,VGA_BLACK);
        ui_text(5,3,"Number (0-9999):",VGA_LGREY,VGA_BLACK);
        long n=t6_num(22,3); if(n<0){vga_clear();return;}
        char w[160]; int o=0;
        if (n > 9999) { for(const char*p="(0-9999 only)";*p;p++) w[o++]=*p; }
        else {
            int v=(int)n;
            if (v>=1000) { o=spell_below_100(v/1000,w,o); for(const char*p=" thousand ";*p;p++) w[o++]=*p; v%=1000; }
            if (v>=100) { for(const char*p=ONES[v/100];*p;p++) w[o++]=*p; for(const char*p=" hundred ";*p;p++) w[o++]=*p; v%=100; }
            if (v>0 || n==0) o=spell_below_100(v,w,o);
        }
        w[o]=0;
        ui_text(5,6,w, n<=9999?VGA_LCYAN:VGA_LRED, VGA_BLACK);
        ui_text(5,9,"any key = again,  q = quit",VGA_DGREY,VGA_BLACK);
        vga_statusbar(" NUM2WORDS   enter a number   Esc quit"); vga_setcursor(0,24);
        if (t6_again()) { vga_clear(); return; }
    }
}

/* ======================= POMODORO ======================= */
void pomodoro_run(void) {
    int work = 1, remain = 25*60, sessions = 0, running = 0; uint32_t last = pit_ticks();
    for (;;) {
        vga_clear();
        ui_panel(2,1,76,22,"Pomodoro Timer",VGA_LCYAN,VGA_BLACK);
        ui_text(5,3, work ? "Focus session" : "Break", work?VGA_LGREEN:VGA_LCYAN, VGA_BLACK);
        char tb[8]; int mm=remain/60, ss=remain%60;
        tb[0]=(char)('0'+(mm/10)%10); tb[1]=(char)('0'+mm%10); tb[2]=':'; tb[3]=(char)('0'+ss/10); tb[4]=(char)('0'+ss%10); tb[5]=0;
        ui_box(28,5,20,3, work?VGA_LGREEN:VGA_LCYAN, VGA_BLACK);
        ui_text(34,6,tb,VGA_WHITE,VGA_BLACK);
        show_n(5,10,"Completed focus sessions: ", sessions, VGA_LGREY);
        ui_text(5,12, running?"running":"paused", running?VGA_LGREEN:VGA_DGREY, VGA_BLACK);
        vga_statusbar(" POMODORO  space start/pause  r reset  s skip  q quit"); vga_setcursor(0,24);
        uint32_t s = pit_ticks();
        while (pit_ticks() - s < 25) {
            if (running && remain>0 && pit_ticks()-last>=100) { remain--; last=pit_ticks(); break; }
            if (running && remain==0) { if (work) sessions++; work=!work; remain=work?25*60:5*60; running=0; break; }
            char c = keyboard_trygetc();
            if (c) {
                if (c=='q'||c==27) { vga_clear(); return; }
                if (c==' ') { running=!running; last=pit_ticks(); }
                else if (c=='r') { remain=work?25*60:5*60; running=0; }
                else if (c=='s') { work=!work; remain=work?25*60:5*60; running=0; }
                break;
            }
            __asm__ volatile("hlt");
        }
    }
}

/* ======================= TIMES TABLE ======================= */
void timestable_run(void) {
    for (;;) {
        vga_clear();
        ui_panel(2,1,76,22,"Times Table",VGA_LCYAN,VGA_BLACK);
        ui_text(5,3,"Number:",VGA_LGREY,VGA_BLACK);
        long n=t6_num(13,3); if(n<0){vga_clear();return;}
        for (int i=1;i<=12;i++) {
            char b[24]; int o=0;
            long v=n; char t[8]; int m=0; if(!v){t[m++]='0';} while(v){t[m++]=(char)('0'+v%10);v/=10;} while(m)b[o++]=t[--m];
            b[o++]=' '; b[o++]='x'; b[o++]=' ';
            b[o++]=(char)('0'+(i/10)%10); if(i<10)o--; b[o++]=(char)('0'+i%10);
            b[o++]=' '; b[o++]='='; b[o++]=' ';
            long r=n*i; char t2[12]; int m2=0; if(!r){t2[m2++]='0';} while(r){t2[m2++]=(char)('0'+r%10);r/=10;} while(m2)b[o++]=t2[--m2];
            b[o]=0;
            ui_text(7, 5+(i-1), b, VGA_LCYAN, VGA_BLACK);
        }
        vga_statusbar(" TIMES TABLE   enter a number   q quit"); vga_setcursor(0,24);
        if (t6_again()) { vga_clear(); return; }
    }
}
