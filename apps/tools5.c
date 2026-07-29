/* tools5.c - converters and math toys (no games), boxed and real. */
#include "tools5.h"
#include "vga.h"
#include "keyboard.h"
#include "printf.h"
#include "string.h"
#include "ui.h"
#include "types.h"

static int t5_text(int x, int y, char *b, int max) {
    int len = 0; b[0] = 0; vga_setcursor(x, y);
    for (;;) {
        char c = keyboard_getc();
        if (c=='\n'||c=='\r') { b[len]=0; return len; }
        else if (c==27) { b[0]=0; return -1; }
        else if (c=='\b') { if (len) { len--; vga_cell(x+len,y,' ',VGA_WHITE,VGA_BLACK); vga_setcursor(x+len,y); } }
        else if (c>=32&&c<127&&len<max-1&&x+len<76) { b[len++]=c; vga_cell(x+len-1,y,c,VGA_WHITE,VGA_BLACK); vga_setcursor(x+len,y); }
    }
}
static long t5_num(int x, int y) { char b[16]; if (t5_text(x,y,b,16)<0) return -1; long v=0; for (int i=0;b[i];i++) if (b[i]>='0'&&b[i]<='9') v=v*10+(b[i]-'0'); return v; }
static int again_or_quit(void) { char k = keyboard_getc(); return (k=='q'||k==27) ? 1 : 0; }

/* ======================= DATE CALCULATOR ======================= */
static long serial(long y, int m, int d) { static int cum[]={0,31,59,90,120,151,181,212,243,273,304,334}; long yy=y; if(m<=2)yy--; long leap=yy/4-yy/100+yy/400; return yy*365+leap+cum[m-1]+d; }
static int dow(long y, int m, int d) { static int t[]={0,3,2,5,0,3,5,1,4,6,2,4}; if(m<3)y--; return (int)((y+y/4-y/100+y/400+t[m-1]+d)%7); }
void datecalc_run(void) {
    static const char *DN[] = { "Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday" };
    for (;;) {
        vga_clear();
        ui_panel(2,1,76,22,"Date Calculator",VGA_LCYAN,VGA_BLACK);
        ui_text(5,3,"Date 1   year :",VGA_LGREY,VGA_BLACK); long y1=t5_num(21,3); if(y1<0){vga_clear();return;}
        ui_text(5,4,"         month:",VGA_LGREY,VGA_BLACK); long m1=t5_num(21,4); if(m1<0){vga_clear();return;}
        ui_text(5,5,"         day  :",VGA_LGREY,VGA_BLACK); long d1=t5_num(21,5); if(d1<0){vga_clear();return;}
        ui_text(5,7,"Date 2   year :",VGA_LGREY,VGA_BLACK); long y2=t5_num(21,7); if(y2<0){vga_clear();return;}
        ui_text(5,8,"         month:",VGA_LGREY,VGA_BLACK); long m2=t5_num(21,8); if(m2<0){vga_clear();return;}
        ui_text(5,9,"         day  :",VGA_LGREY,VGA_BLACK); long d2=t5_num(21,9); if(d2<0){vga_clear();return;}
        long diff = serial(y2,(int)m2,(int)d2) - serial(y1,(int)m1,(int)d1); if (diff<0) diff=-diff;
        char b[40]; int o=0; for(const char*p="Days between : ";*p;p++)b[o++]=*p; { long v=diff; char t[12]; int n=0; if(!v)t[n++]='0'; while(v){t[n++]=(char)('0'+v%10);v/=10;} while(n)b[o++]=t[--n]; } b[o]=0;
        ui_text(5,12,b,VGA_LGREEN,VGA_BLACK);
        ui_text(5,13,"Date 1 is a ",VGA_LGREY,VGA_BLACK); ui_text(17,13,DN[dow(y1,(int)m1,(int)d1)],VGA_LCYAN,VGA_BLACK);
        ui_text(5,14,"Date 2 is a ",VGA_LGREY,VGA_BLACK); ui_text(17,14,DN[dow(y2,(int)m2,(int)d2)],VGA_LCYAN,VGA_BLACK);
        ui_text(5,17,"any key = again,  q = quit",VGA_DGREY,VGA_BLACK);
        vga_statusbar(" DATE   enter dates   Esc quit"); vga_setcursor(0,24);
        if (again_or_quit()) { vga_clear(); return; }
    }
}

/* ======================= ROMAN NUMERALS ======================= */
void roman_run(void) {
    static const int rv[] = {1000,900,500,400,100,90,50,40,10,9,5,4,1};
    static const char *rs[] = {"M","CM","D","CD","C","XC","L","XL","X","IX","V","IV","I"};
    for (;;) {
        vga_clear();
        ui_panel(2,1,76,22,"Roman Numerals",VGA_LCYAN,VGA_BLACK);
        ui_text(5,3,"Number (1-3999):",VGA_LGREY,VGA_BLACK);
        long n=t5_num(22,3); if(n<0){vga_clear();return;}
        char r[40]; int o=0; long nn=n;
        if (nn>0 && nn<4000) for (int i=0;i<13;i++) while (nn>=rv[i]) { for (const char*p=rs[i];*p;p++) r[o++]=*p; nn-=rv[i]; }
        r[o]=0;
        ui_text(5,6,"Roman :",VGA_LGREY,VGA_BLACK);
        ui_text(13,6, (n>0&&n<4000)?r:"(1-3999 only)", (n>0&&n<4000)?VGA_LCYAN:VGA_LRED, VGA_BLACK);
        ui_text(5,9,"any key = again,  q = quit",VGA_DGREY,VGA_BLACK);
        vga_statusbar(" ROMAN   enter a number   Esc quit"); vga_setcursor(0,24);
        if (again_or_quit()) { vga_clear(); return; }
    }
}

/* ======================= CIPHER ======================= */
void cipher_run(void) {
    for (;;) {
        vga_clear();
        ui_panel(2,1,76,22,"Cipher (Caesar / ROT13)",VGA_LCYAN,VGA_BLACK);
        ui_text(5,3,"Text :",VGA_LGREY,VGA_BLACK); char txt[60]; if(t5_text(12,3,txt,60)<0){vga_clear();return;}
        ui_text(5,4,"Shift:",VGA_LGREY,VGA_BLACK); long sh=t5_num(12,4); if(sh<0){vga_clear();return;} sh%=26;
        char ca[60], r13[60]; int o=0;
        for (int i=0;txt[i];i++){ char c=txt[i]; if(c>='a'&&c<='z') ca[o]=(char)('a'+(c-'a'+sh)%26); else if(c>='A'&&c<='Z') ca[o]=(char)('A'+(c-'A'+sh)%26); else ca[o]=c; o++; } ca[o]=0;
        o=0; for (int i=0;txt[i];i++){ char c=txt[i]; if(c>='a'&&c<='z') r13[o]=(char)('a'+(c-'a'+13)%26); else if(c>='A'&&c<='Z') r13[o]=(char)('A'+(c-'A'+13)%26); else r13[o]=c; o++; } r13[o]=0;
        ui_text(5,7,"Caesar:",VGA_LGREY,VGA_BLACK); ui_text(13,7,ca,VGA_LCYAN,VGA_BLACK);
        ui_text(5,8,"ROT13 :",VGA_LGREY,VGA_BLACK); ui_text(13,8,r13,VGA_LCYAN,VGA_BLACK);
        ui_text(5,11,"any key = again,  q = quit",VGA_DGREY,VGA_BLACK);
        vga_statusbar(" CIPHER   text + shift   Esc quit"); vga_setcursor(0,24);
        if (again_or_quit()) { vga_clear(); return; }
    }
}

/* ======================= MORSE ======================= */
static const char *MORSE_A[] = {".-","-...","-.-.","-..",".","..-.","--.","....","..",".---","-.-",".-..","--","-.","---",".--.","--.-",".-.","...","-","..-","...-",".--","-..-","-.--","--.."};
static const char *MORSE_D[] = {"-----",".----","..---","...--","....-",".....","-....","--...","---..","----."};
void morse_run(void) {
    for (;;) {
        vga_clear();
        ui_panel(2,1,76,22,"Morse Code",VGA_LCYAN,VGA_BLACK);
        ui_text(5,3,"Text:",VGA_LGREY,VGA_BLACK);
        char txt[40]; if (t5_text(11,3,txt,40)<0) { vga_clear(); return; }
        int x = 5, y = 6;
        vga_setcolor(VGA_LCYAN, VGA_BLACK);
        for (int i=0; txt[i]; i++) {
            char c=txt[i]; const char *m=0;
            if (c>='a'&&c<='z') m=MORSE_A[c-'a']; else if (c>='A'&&c<='Z') m=MORSE_A[c-'A']; else if (c>='0'&&c<='9') m=MORSE_D[c-'0'];
            if (c==' ') { x += 2; if (x>72){x=5;y++;} continue; }
            if (!m) continue;
            for (int k=0; m[k]; k++) { if (x>74){x=5;y++;} vga_cell(x++,y,m[k],VGA_LCYAN,VGA_BLACK); }
            x++; if (x>72){x=5;y++;}
        }
        vga_setcolor(VGA_LGREY, VGA_BLACK);
        ui_text(5,18,"any key = again,  q = quit",VGA_DGREY,VGA_BLACK);
        vga_statusbar(" MORSE   enter text   Esc quit"); vga_setcursor(0,24);
        if (again_or_quit()) { vga_clear(); return; }
    }
}

/* ======================= PRIMES ======================= */
void primes_run(void) {
    for (;;) {
        vga_clear();
        ui_panel(2,1,76,22,"Primes",VGA_LCYAN,VGA_BLACK);
        ui_text(5,3,"N:",VGA_LGREY,VGA_BLACK);
        long n=t5_num(8,3); if(n<0){vga_clear();return;}
        int isp = n>=2; for (long d=2; d*d<=n; d++) if (n%d==0) { isp=0; break; }
        ui_text(5,5,"N is",VGA_LGREY,VGA_BLACK); ui_text(10,5, isp?"prime":"not prime", isp?VGA_LGREEN:VGA_YELLOW, VGA_BLACK);
        ui_text(5,7,"Primes up to N:",VGA_DGREY,VGA_BLACK);
        int x=5,y=8;
        for (long v=2; v<=n && y<=20; v++) {
            int p=1; for (long d=2; d*d<=v; d++) if (v%d==0){p=0;break;}
            if (!p) continue;
            char t[8]; int m=0; long vv=v; while(vv){t[m++]=(char)('0'+vv%10);vv/=10;}
            if (x+m>74){x=5;y++; if(y>20)break;}
            while(m) vga_cell(x++,y,t[--m],VGA_LCYAN,VGA_BLACK);
            x+=2;
        }
        ui_text(5,22,"any key = again,  q = quit",VGA_DGREY,VGA_BLACK);
        vga_statusbar(" PRIMES   enter N   Esc quit"); vga_setcursor(0,24);
        if (again_or_quit()) { vga_clear(); return; }
    }
}
