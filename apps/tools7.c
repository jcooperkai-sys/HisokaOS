/* tools7.c - more real boxed apps (no games): math + text + fun. */
#include "tools7.h"
#include "vga.h"
#include "keyboard.h"
#include "printf.h"
#include "string.h"
#include "pit.h"
#include "ui.h"
#include "types.h"

static uint32_t rng7;
static uint32_t rnd7(void) { rng7 = rng7*1103515245u + 12345u; return (rng7 >> 16) & 0x7fff; }
static int t7_text(int x, int y, char *b, int max) {
    int len = 0; b[0] = 0; vga_setcursor(x, y);
    for (;;) { char c = keyboard_getc();
        if (c=='\n'||c=='\r') { b[len]=0; return len; }
        else if (c==27) { b[0]=0; return -1; }
        else if (c=='\b') { if (len) { len--; vga_cell(x+len,y,' ',VGA_WHITE,VGA_BLACK); vga_setcursor(x+len,y); } }
        else if (c>=32&&c<127&&len<max-1&&x+len<76) { b[len++]=c; vga_cell(x+len-1,y,c,VGA_WHITE,VGA_BLACK); vga_setcursor(x+len,y); } }
}
static long t7_num(int x, int y) { char b[16]; if (t7_text(x,y,b,16)<0) return -1; long v=0; for (int i=0;b[i];i++) if(b[i]>='0'&&b[i]<='9') v=v*10+(b[i]-'0'); return v; }
static int t7_again(void) { char k=keyboard_getc(); return (k=='q'||k==27); }
static void show7(int x, int y, const char *label, long v, uint8_t fg) {
    char b[48]; int o=0; for (const char*p=label;*p;p++) b[o++]=*p;
    long av=v<0?-v:v; char t[16]; int m=0; if(!av)t[m++]='0'; while(av){t[m++]=(char)('0'+av%10);av/=10;} if(v<0)b[o++]='-'; while(m)b[o++]=t[--m]; b[o]=0;
    ui_text(x,y,b,fg,VGA_BLACK);
}

void gcdlcm_run(void) {
    for (;;) {
        vga_clear(); ui_panel(2,1,76,22,"GCD / LCM",VGA_LCYAN,VGA_BLACK);
        ui_text(5,3,"A :",VGA_LGREY,VGA_BLACK); long a=t7_num(9,3); if(a<0){vga_clear();return;}
        ui_text(5,4,"B :",VGA_LGREY,VGA_BLACK); long b=t7_num(9,4); if(b<0){vga_clear();return;}
        long x=a,y=b; while(y){long t=x%y;x=y;y=t;} long g=x?x:1;
        show7(5,7,"GCD : ", g, VGA_LGREEN);
        show7(5,8,"LCM : ", g? a/g*b : 0, VGA_LCYAN);
        ui_text(5,11,"any key = again,  q = quit",VGA_DGREY,VGA_BLACK);
        vga_statusbar(" GCD/LCM   enter A and B   Esc quit"); vga_setcursor(0,24);
        if (t7_again()) { vga_clear(); return; }
    }
}

void factorial_run(void) {
    for (;;) {
        vga_clear(); ui_panel(2,1,76,22,"Factorial / Combinations",VGA_LCYAN,VGA_BLACK);
        ui_text(5,3,"n :",VGA_LGREY,VGA_BLACK); long n=t7_num(9,3); if(n<0){vga_clear();return;}
        ui_text(5,4,"r :",VGA_LGREY,VGA_BLACK); long r=t7_num(9,4); if(r<0){vga_clear();return;}
        long fn=1; int over=0; for(long i=2;i<=n;i++){ fn*=i; if(n>12)over=1; }
        if (over) ui_text(5,7,"n! too big for 32-bit (n<=12)",VGA_LRED,VGA_BLACK);
        else show7(5,7,"n!  : ", fn, VGA_LGREEN);
        if (r<=n && n<=20) {
            long ncr=1; for(long i=0;i<r;i++) ncr=ncr*(n-i)/(i+1);
            long npr=1; for(long i=0;i<r;i++) npr*=(n-i);
            show7(5,9,"nCr : ", ncr, VGA_LCYAN);
            show7(5,10,"nPr : ", npr, VGA_LCYAN);
        }
        ui_text(5,13,"any key = again,  q = quit",VGA_DGREY,VGA_BLACK);
        vga_statusbar(" FACTORIAL   enter n and r   Esc quit"); vga_setcursor(0,24);
        if (t7_again()) { vga_clear(); return; }
    }
}

void fibonacci_run(void) {
    for (;;) {
        vga_clear(); ui_panel(2,1,76,22,"Fibonacci",VGA_LCYAN,VGA_BLACK);
        ui_text(5,3,"How many:",VGA_LGREY,VGA_BLACK); long n=t7_num(15,3); if(n<0){vga_clear();return;}
        long a=0,b=1; int x=5,y=6;
        for (long i=0;i<n && y<=20;i++) {
            char t[16]; int m=0; long v=a; if(!v)t[m++]='0'; while(v){t[m++]=(char)('0'+v%10);v/=10;}
            if (x+m>74){x=5;y++; if(y>20)break;}
            while(m) vga_cell(x++,y,t[--m],VGA_LCYAN,VGA_BLACK); x+=2;
            long nx=a+b; a=b; b=nx;
        }
        ui_text(5,22,"any key = again,  q = quit",VGA_DGREY,VGA_BLACK);
        vga_statusbar(" FIBONACCI   enter count   Esc quit"); vga_setcursor(0,24);
        if (t7_again()) { vga_clear(); return; }
    }
}

void reverse_run(void) {
    for (;;) {
        vga_clear(); ui_panel(2,1,76,22,"Reverse / Palindrome",VGA_LCYAN,VGA_BLACK);
        ui_text(5,3,"Text:",VGA_LGREY,VGA_BLACK); char t[64]; if(t7_text(11,3,t,64)<0){vga_clear();return;}
        int n=(int)strlen(t); char r[64]; for(int i=0;i<n;i++) r[i]=t[n-1-i]; r[n]=0;
        ui_text(5,6,"Reversed:",VGA_LGREY,VGA_BLACK); ui_text(15,6,r,VGA_LCYAN,VGA_BLACK);
        int pal=1; for(int i=0,j=n-1;i<j;i++,j--){ char a=t[i],b=t[j]; if(a>='A'&&a<='Z')a+=32; if(b>='A'&&b<='Z')b+=32; if(a!=b){pal=0;break;} }
        ui_text(5,8,"Palindrome:",VGA_LGREY,VGA_BLACK); ui_text(17,8, (n&&pal)?"yes":"no", (n&&pal)?VGA_LGREEN:VGA_YELLOW, VGA_BLACK);
        ui_text(5,11,"any key = again,  q = quit",VGA_DGREY,VGA_BLACK);
        vga_statusbar(" REVERSE   enter text   Esc quit"); vga_setcursor(0,24);
        if (t7_again()) { vga_clear(); return; }
    }
}

void lorem_run(void) {
    static const char *W[] = {"lorem","ipsum","dolor","sit","amet","consectetur","adipiscing","elit","sed","do",
        "eiusmod","tempor","incididunt","ut","labore","et","dolore","magna","aliqua","enim","ad","minim","veniam"};
    rng7 = pit_ticks() + 3;
    for (;;) {
        vga_clear(); ui_panel(2,1,76,22,"Lorem Ipsum",VGA_LCYAN,VGA_BLACK);
        ui_text(5,3,"How many words:",VGA_LGREY,VGA_BLACK); long n=t7_num(21,3); if(n<0){vga_clear();return;}
        int x=5,y=6;
        for (long i=0;i<n && y<=20;i++) {
            const char *w=W[rnd7()%23];
            int wl=(int)strlen(w); if (x+wl>74){x=5;y++; if(y>20)break;}
            for (int k=0;w[k];k++) vga_cell(x++,y,w[k],VGA_LGREY,VGA_BLACK); x++;
        }
        ui_text(5,22,"any key = again,  q = quit",VGA_DGREY,VGA_BLACK);
        vga_statusbar(" LOREM   enter word count   Esc quit"); vga_setcursor(0,24);
        if (t7_again()) { vga_clear(); return; }
    }
}

void eightball_run(void) {
    static const char *ANS[] = {"It is certain","Without a doubt","Yes, definitely","Most likely","Outlook good",
        "Signs point to yes","Reply hazy, try again","Ask again later","Cannot predict now","Don't count on it",
        "My reply is no","Very doubtful","Concentrate and ask again"};
    rng7 = pit_ticks() + 11;
    for (;;) {
        vga_clear(); ui_panel(2,1,76,22,"Magic 8-Ball",VGA_LCYAN,VGA_BLACK);
        ui_text(5,3,"Ask a yes/no question:",VGA_LGREY,VGA_BLACK);
        char q[64]; if (t7_text(5,4,q,64)<0) { vga_clear(); return; }
        rng7 += pit_ticks();
        ui_text(5,7,"The 8-ball says:",VGA_DGREY,VGA_BLACK);
        ui_text(5,9,ANS[rnd7()%13],VGA_LMAGENTA,VGA_BLACK);
        ui_text(5,12,"any key = ask again,  q = quit",VGA_DGREY,VGA_BLACK);
        vga_statusbar(" 8-BALL   ask a question   Esc quit"); vga_setcursor(0,24);
        if (t7_again()) { vga_clear(); return; }
    }
}

void discount_run(void) {
    for (;;) {
        vga_clear(); ui_panel(2,1,76,22,"Discount Calculator",VGA_LCYAN,VGA_BLACK);
        ui_text(5,3,"Price      :",VGA_LGREY,VGA_BLACK); long p=t7_num(18,3); if(p<0){vga_clear();return;}
        ui_text(5,4,"Discount % :",VGA_LGREY,VGA_BLACK); long d=t7_num(18,4); if(d<0){vga_clear();return;}
        long save=p*d/100;
        show7(5,7,"You save  : ", save, VGA_LGREEN);
        show7(5,8,"Final price: ", p-save, VGA_WHITE);
        ui_text(5,11,"any key = again,  q = quit",VGA_DGREY,VGA_BLACK);
        vga_statusbar(" DISCOUNT   price + percent   Esc quit"); vga_setcursor(0,24);
        if (t7_again()) { vga_clear(); return; }
    }
}
