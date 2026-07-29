/* tools8.c - the apps that take HisokaOS to 60. Boxed, real, integer math. */
#include "tools8.h"
#include "vga.h"
#include "keyboard.h"
#include "printf.h"
#include "string.h"
#include "ramfs.h"
#include "rtc.h"
#include "gfx.h"
#include "pit.h"
#include "ui.h"
#include "types.h"

static int t8_text(int x, int y, char *b, int max) {
    int len = 0; b[0] = 0; vga_setcursor(x, y);
    for (;;) { char c = keyboard_getc();
        if (c=='\n'||c=='\r') { b[len]=0; return len; }
        else if (c==27) { b[0]=0; return -1; }
        else if (c=='\b') { if (len) { len--; vga_cell(x+len,y,' ',VGA_WHITE,VGA_BLACK); vga_setcursor(x+len,y); } }
        else if (c>=32&&c<127&&len<max-1&&x+len<76) { b[len++]=c; vga_cell(x+len-1,y,c,VGA_WHITE,VGA_BLACK); vga_setcursor(x+len,y); } }
}
static long t8_num(int x, int y) { char b[16]; if (t8_text(x,y,b,16)<0) return -1; long v=0; for (int i=0;b[i];i++) if(b[i]>='0'&&b[i]<='9') v=v*10+(b[i]-'0'); return v; }
static int t8_again(void) { char k=keyboard_getc(); return (k=='q'||k==27); }
static char lc8(char c) { return (c>='A'&&c<='Z')?(char)(c+32):c; }
static void show8(int x, int y, const char *label, long v, uint8_t fg) {
    char b[48]; int o=0; for (const char*p=label;*p;p++) b[o++]=*p;
    long av=v<0?-v:v; char t[16]; int m=0; if(!av)t[m++]='0'; while(av){t[m++]=(char)('0'+av%10);av/=10;} if(v<0)b[o++]='-'; while(m)b[o++]=t[--m]; b[o]=0;
    ui_text(x,y,b,fg,VGA_BLACK);
}

/* ======================= CURRENCY ======================= */
void currency_run(void) {
    static const char *cur[] = {"usd","eur","gbp","jpy","cad","aud","inr"};
    static const long rate100[] = {100, 92, 79, 14900, 136, 152, 8300};  /* units per 1 USD x100 */
    for (;;) {
        vga_clear(); ui_panel(2,1,76,22,"Currency Converter",VGA_LCYAN,VGA_BLACK);
        ui_text(5,3,"Codes: usd eur gbp jpy cad aud inr",VGA_DGREY,VGA_BLACK);
        ui_text(5,5,"Amount :",VGA_LGREY,VGA_BLACK); long amt=t8_num(14,5); if(amt<0){vga_clear();return;}
        char f[8],t[8];
        ui_text(5,6,"From   :",VGA_LGREY,VGA_BLACK); if(t8_text(14,6,f,8)<0){vga_clear();return;} for(int i=0;f[i];i++)f[i]=lc8(f[i]);
        ui_text(5,7,"To     :",VGA_LGREY,VGA_BLACK); if(t8_text(14,7,t,8)<0){vga_clear();return;} for(int i=0;t[i];i++)t[i]=lc8(t[i]);
        int fi=-1,ti=-1; for(int i=0;i<7;i++){ if(!strcmp(f,cur[i]))fi=i; if(!strcmp(t,cur[i]))ti=i; }
        if (fi<0||ti<0) ui_text(5,10,"unknown currency code",VGA_LRED,VGA_BLACK);
        else { long usd = amt*100/rate100[fi]; long out = usd*rate100[ti]/100; show8(5,10,"Result : ", out, VGA_LGREEN); ui_text(5,11,"(approximate fixed rates)",VGA_DGREY,VGA_BLACK); }
        ui_text(5,14,"any key = again,  q = quit",VGA_DGREY,VGA_BLACK);
        vga_statusbar(" CURRENCY   amount + codes   Esc quit"); vga_setcursor(0,24);
        if (t8_again()) { vga_clear(); return; }
    }
}

/* ======================= SAVINGS GOAL ======================= */
void savings_run(void) {
    for (;;) {
        vga_clear(); ui_panel(2,1,76,22,"Savings Goal",VGA_LCYAN,VGA_BLACK);
        ui_text(5,3,"Target  :",VGA_LGREY,VGA_BLACK); long tg=t8_num(15,3); if(tg<0){vga_clear();return;}
        ui_text(5,4,"Saved   :",VGA_LGREY,VGA_BLACK); long sv=t8_num(15,4); if(sv<0){vga_clear();return;}
        ui_text(5,5,"Per month:",VGA_LGREY,VGA_BLACK); long pm=t8_num(15,5); if(pm<0){vga_clear();return;}
        long need = tg-sv; if (need<0) need=0;
        long months = pm? (need + pm - 1)/pm : 0;
        show8(5,8,"Still need : ", need, VGA_WHITE);
        show8(5,9,"Months     : ", months, VGA_LGREEN);
        show8(5,10,"Years (x10): ", months*10/12, VGA_LCYAN);
        ui_text(5,13,"any key = again,  q = quit",VGA_DGREY,VGA_BLACK);
        vga_statusbar(" SAVINGS   enter numbers   Esc quit"); vga_setcursor(0,24);
        if (t8_again()) { vga_clear(); return; }
    }
}

/* ======================= COLOR PICKER ======================= */
void colorpick_run(void) {
    for (;;) {
        vga_clear(); ui_panel(2,1,76,22,"Color Picker",VGA_LCYAN,VGA_BLACK);
        ui_text(5,3,"R (0-255):",VGA_LGREY,VGA_BLACK); long r=t8_num(16,3); if(r<0){vga_clear();return;} if(r>255)r=255;
        ui_text(5,4,"G (0-255):",VGA_LGREY,VGA_BLACK); long g=t8_num(16,4); if(g<0){vga_clear();return;} if(g>255)g=255;
        ui_text(5,5,"B (0-255):",VGA_LGREY,VGA_BLACK); long b=t8_num(16,5); if(b<0){vga_clear();return;} if(b>255)b=255;
        const char *H = "0123456789abcdef";
        char hex[8]; hex[0]='#'; hex[1]=H[(r>>4)&15]; hex[2]=H[r&15]; hex[3]=H[(g>>4)&15]; hex[4]=H[g&15]; hex[5]=H[(b>>4)&15]; hex[6]=H[b&15]; hex[7]=0;
        ui_text(5,8,"Hex:",VGA_LGREY,VGA_BLACK); ui_text(10,8,hex,VGA_LCYAN,VGA_BLACK);
        ui_text(5,11,"p = preview in full color,  any key = again,  q = quit",VGA_DGREY,VGA_BLACK);
        vga_statusbar(" COLOR   enter RGB   p preview   Esc quit"); vga_setcursor(0,24);
        char k = keyboard_getc();
        if (k=='q'||k==27) { vga_clear(); return; }
        if (k=='p' && gfx_available()) {
            gfx_enter(1024,768);
            gfx_fill(0,0,1024,768, ((uint32_t)r<<16)|((uint32_t)g<<8)|(uint32_t)b);
            keyboard_getc(); gfx_exit();
        }
    }
}

/* ======================= BINARY CLOCK ======================= */
static void bin8(int x, int y, int val, int bits) {
    for (int i = 0; i < bits; i++) {
        int bit = (val >> (bits-1-i)) & 1;
        vga_cell(x + i*2, y, bit ? (char)0xDB : (char)0xB0, bit ? VGA_LGREEN : VGA_DGREY, VGA_BLACK);
    }
}
void binclock_run(void) {
    for (;;) {
        rtc_time_t t; rtc_now(&t);
        vga_clear(); ui_panel(2,1,76,22,"Binary Clock",VGA_LCYAN,VGA_BLACK);
        ui_text(5,5,"Hours  ",VGA_LGREY,VGA_BLACK); bin8(15,5,t.hour,6);
        ui_text(5,7,"Minutes",VGA_LGREY,VGA_BLACK); bin8(15,7,t.min,6);
        ui_text(5,9,"Seconds",VGA_LGREY,VGA_BLACK); bin8(15,9,t.sec,6);
        char d[10]; d[0]=(char)('0'+t.hour/10); d[1]=(char)('0'+t.hour%10); d[2]=':'; d[3]=(char)('0'+t.min/10); d[4]=(char)('0'+t.min%10); d[5]=':'; d[6]=(char)('0'+t.sec/10); d[7]=(char)('0'+t.sec%10); d[8]=0;
        ui_text(15,12,d,VGA_DGREY,VGA_BLACK);
        vga_statusbar(" BINARY CLOCK   live   q quit"); vga_setcursor(0,24);
        uint32_t s = pit_ticks();
        while (pit_ticks()-s < 40) { char c=keyboard_trygetc(); if(c=='q'||c==27){vga_clear();return;} __asm__ volatile("hlt"); }
    }
}

/* ======================= PIG LATIN ======================= */
static int is_vowel8(char c) { c=lc8(c); return c=='a'||c=='e'||c=='i'||c=='o'||c=='u'; }
void piglatin_run(void) {
    for (;;) {
        vga_clear(); ui_panel(2,1,76,22,"Pig Latin",VGA_LCYAN,VGA_BLACK);
        ui_text(5,3,"Text:",VGA_LGREY,VGA_BLACK); char t[80]; if(t8_text(11,3,t,80)<0){vga_clear();return;}
        char out[160]; int o=0; int i=0;
        while (t[i]) {
            while (t[i]==' ') { out[o++]=' '; i++; }
            if (!t[i]) break;
            int s=i; while (t[i] && t[i]!=' ') i++;
            int e=i;
            if (is_vowel8(t[s])) { for(int k=s;k<e;k++) out[o++]=t[k]; out[o++]='w'; out[o++]='a'; out[o++]='y'; }
            else {
                int c=s; while (c<e && !is_vowel8(t[c])) c++;
                for (int k=c;k<e;k++) out[o++]=t[k];
                for (int k=s;k<c;k++) out[o++]=t[k];
                out[o++]='a'; out[o++]='y';
            }
        }
        out[o]=0;
        vga_setcolor(VGA_LCYAN,VGA_BLACK);
        { int x=5,y=6; for (int k=0;out[k];k++){ if(out[k]=='\n'||x>74){x=5;y++;} vga_cell(x++,y,out[k],VGA_LCYAN,VGA_BLACK); } }
        vga_setcolor(VGA_LGREY,VGA_BLACK);
        ui_text(5,18,"any key = again,  q = quit",VGA_DGREY,VGA_BLACK);
        vga_statusbar(" PIG LATIN   enter text   Esc quit"); vga_setcursor(0,24);
        if (t8_again()) { vga_clear(); return; }
    }
}

/* ======================= WATER TRACKER ======================= */
static int water_load(void) { fs_file_t *f=fs_find("/home/water.txt"); if(!f||!f->data) return 0; int v=0; for(size_t i=0;i<f->len;i++){ char c=(char)f->data[i]; if(c<'0'||c>'9')break; v=v*10+(c-'0'); } return v; }
static void water_save(int v) { char b[8]; int n=0; if(!v)b[n++]='0'; else{char t[8];int m=0;int x=v;while(x){t[m++]=(char)('0'+x%10);x/=10;}while(m)b[n++]=t[--m];} b[n++]='\n'; fs_write("/home/water.txt",b,(size_t)n); }
void water_run(void) {
    int glasses = water_load();
    const int goal = 8;
    for (;;) {
        vga_clear(); ui_panel(2,1,76,22,"Water Tracker",VGA_LCYAN,VGA_BLACK);
        show8(5,3,"Glasses today : ", glasses, VGA_LCYAN);
        show8(5,4,"Goal          : ", goal, VGA_LGREY);
        for (int i=0;i<goal;i++) vga_cell(5+i*2, 7, i<glasses ? (char)0xDB : (char)0xB0, i<glasses?VGA_LBLUE:VGA_DGREY, VGA_BLACK);
        ui_text(5, 9, glasses>=goal ? "Goal reached! Nice." : "Keep drinking water.", glasses>=goal?VGA_LGREEN:VGA_DGREY, VGA_BLACK);
        vga_statusbar(" WATER   + add glass   - remove   r reset   q quit"); vga_setcursor(0,24);
        char k = keyboard_getc();
        if (k=='q'||k==27) { water_save(glasses); break; }
        else if (k=='+'||k=='='||k==' ') { if (glasses<20) glasses++; water_save(glasses); }
        else if (k=='-'||k=='_') { if (glasses>0) glasses--; water_save(glasses); }
        else if (k=='r') { glasses=0; water_save(0); }
    }
    vga_clear(); vga_setcolor(VGA_LCYAN,VGA_BLACK); kputs("Water tracker saved.\n"); vga_setcolor(VGA_LGREY,VGA_BLACK);
}

/* ======================= LEETSPEAK ======================= */
void leet_run(void) {
    for (;;) {
        vga_clear(); ui_panel(2,1,76,22,"Leetspeak",VGA_LCYAN,VGA_BLACK);
        ui_text(5,3,"Text:",VGA_LGREY,VGA_BLACK); char t[80]; if(t8_text(11,3,t,80)<0){vga_clear();return;}
        char out[80]; int o=0;
        for (int i=0;t[i];i++) { char c=lc8(t[i]);
            char r = c=='a'?'4':c=='e'?'3':c=='i'?'1':c=='o'?'0':c=='t'?'7':c=='s'?'5':c=='g'?'9':c=='b'?'8':t[i];
            out[o++]=r; }
        out[o]=0;
        ui_text(5,6,"l33t:",VGA_LGREY,VGA_BLACK);
        { int x=11,y=6; for (int k=0;out[k];k++){ if(x>74){x=5;y++;} vga_cell(x++,y,out[k],VGA_LGREEN,VGA_BLACK); } }
        ui_text(5,9,"any key = again,  q = quit",VGA_DGREY,VGA_BLACK);
        vga_statusbar(" LEET   enter text   Esc quit"); vga_setcursor(0,24);
        if (t8_again()) { vga_clear(); return; }
    }
}
