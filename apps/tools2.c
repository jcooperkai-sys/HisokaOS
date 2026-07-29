/* tools2.c - more real utility apps (no games). Each is fully operable
 * (features for features) with create/edit/delete/persist + on-screen keys. */
#include "tools.h"
#include "vga.h"
#include "keyboard.h"
#include "printf.h"
#include "ramfs.h"
#include "string.h"
#include "pit.h"
#include "rtc.h"
#include "browser.h"
#include "ui.h"
#include "types.h"

/* ---- shared ---- */
static int t2_prompt(int row, const char *label, char *buf, int max, const char *preset) {
    for (int x = 1; x <= 78; x++) vga_cell(x, row, ' ', VGA_WHITE, VGA_BLACK);
    int px = 2; for (int i = 0; label[i]; i++) vga_cell(px++, row, label[i], VGA_YELLOW, VGA_BLACK);
    int len = 0; buf[0] = 0; int sx = px;
    if (preset) for (int i = 0; preset[i] && len < max-1; i++) { buf[len]=preset[i]; vga_cell(sx+len,row,preset[i],VGA_WHITE,VGA_BLACK); len++; }
    buf[len] = 0; vga_setcursor(sx+len, row);
    for (;;) {
        char c = keyboard_getc();
        if (c=='\n'||c=='\r') { buf[len]=0; return len; }
        else if (c==27) { buf[0]=0; return -1; }
        else if (c=='\b') { if (len) { len--; vga_cell(sx+len,row,' ',VGA_WHITE,VGA_BLACK); vga_setcursor(sx+len,row); } }
        else if (c>=32&&c<127&&len<max-1) { buf[len++]=c; vga_cell(sx+len-1,row,c,VGA_WHITE,VGA_BLACK); vga_setcursor(sx+len,row); }
    }
}
static long t2_atol(const char *s) { long v=0; int neg=0; while(*s==' ')s++; if(*s=='-'){neg=1;s++;} while(*s>='0'&&*s<='9') v=v*10+(*s++-'0'); return neg?-v:v; }
static int ci_has(const char *hay, const char *needle) {  /* case-insensitive substring */
    if (!*needle) return 1;
    for (const char *h = hay; *h; h++) {
        const char *a=h, *b=needle;
        while (*a && *b) { char x=*a,y=*b; if(x>='A'&&x<='Z')x+=32; if(y>='A'&&y<='Z')y+=32; if(x!=y)break; a++; b++; }
        if (!*b) return 1;
    }
    return 0;
}

/* ======================= CALENDAR ======================= */
static int cal_dow(int y, int m, int d) { static int t[]={0,3,2,5,0,3,5,1,4,6,2,4}; if(m<3)y--; return (y+y/4-y/100+y/400+t[m-1]+d)%7; }
static int cal_mdays(int y, int m) { static int dd[]={31,28,31,30,31,30,31,31,30,31,30,31}; if(m==2&&((y%4==0&&y%100!=0)||y%400==0)) return 29; return dd[m-1]; }
void calendar_run(void) {
    static const char *MON[] = {"","January","February","March","April","May","June","July","August","September","October","November","December"};
    rtc_time_t now; rtc_now(&now);
    int y = now.year, m = now.month;
    for (;;) {
        vga_clear();
        int bx = 14, by = 3, bw = 52, bh = 13;
        ui_panel(bx, by, bw, bh, "Calendar", VGA_LCYAN, VGA_BLACK);
        char hdr[24]; int n = 0;
        for (const char *p = MON[m]; *p; p++) hdr[n++] = *p; hdr[n++] = ' ';
        char tmp[8]; int tn = 0, yy = y; while (yy) { tmp[tn++] = (char)('0'+yy%10); yy/=10; } while (tn) hdr[n++] = tmp[--tn]; hdr[n] = 0;
        ui_center(by+1, bx, bw, hdr, VGA_YELLOW, VGA_BLACK);
        ui_text(bx+4, by+3, "Su  Mo  Tu  We  Th  Fr  Sa", VGA_DGREY, VGA_BLACK);
        int first = cal_dow(y, m, 1), days = cal_mdays(y, m);
        int col = first, row = by+4;
        for (int d = 1; d <= days; d++) {
            int istoday = (y==now.year && m==now.month && d==now.day);
            uint8_t fg = istoday ? VGA_BLACK : VGA_LGREY, bg = istoday ? VGA_LGREEN : VGA_BLACK;
            int cx = bx+4 + col*4;
            if (d < 10) vga_cell(cx+1, row, (char)('0'+d), fg, bg);
            else { vga_cell(cx, row, (char)('0'+d/10), fg, bg); vga_cell(cx+1, row, (char)('0'+d%10), fg, bg); }
            if (++col > 6) { col = 0; row++; }
        }
        vga_statusbar(" CALENDAR   [ prev month   ] next month   t today   q quit");
        vga_setcursor(0, 24);
        char k = keyboard_getc();
        if (k=='q'||k==27) break;
        else if (k=='[') { if (--m < 1) { m = 12; y--; } }
        else if (k==']') { if (++m > 12) { m = 1; y++; } }
        else if (k=='t') { y = now.year; m = now.month; }
    }
    vga_clear(); vga_setcolor(VGA_LCYAN,VGA_BLACK); kputs("Calendar closed.\n"); vga_setcolor(VGA_LGREY,VGA_BLACK);
}

/* ======================= a generic record list (name|f2[|f3]) ======================= */
#define RMAX 64
#define RLEN 80
static char r_a[RMAX][RLEN], r_b[RMAX][RLEN], r_c[RMAX][RLEN];
static int  r_n;
static void rec_load(const char *path, int fields) {
    r_n = 0; fs_file_t *f = fs_find(path); if (!f || f->is_dir) return;
    char line[240]; int li = 0;
    for (size_t k = 0; k <= f->len && r_n < RMAX; k++) {
        char c = (k < f->len) ? (char)f->data[k] : '\n';
        if (c == '\n') {
            line[li] = 0;
            if (li > 0) {
                int p = 0, fld = 0; char *dst[3] = { r_a[r_n], r_b[r_n], r_c[r_n] };
                r_a[r_n][0]=r_b[r_n][0]=r_c[r_n][0]=0;
                int o = 0;
                for (int i = 0; i <= li; i++) {
                    if (line[i] == '|' || line[i] == 0) { dst[fld][o]=0; if(++fld>=fields)break; o=0; p=i+1; }
                    else if (o < RLEN-1 && fld < 3) dst[fld][o++] = line[i];
                }
                (void)p;
                r_n++;
            }
            li = 0;
        } else if (li < 239) line[li++] = c;
    }
}
static void rec_save(const char *path, int fields) {
    static char out[RMAX*(RLEN*3+4)]; int o = 0;
    for (int i = 0; i < r_n; i++) {
        for (int k = 0; r_a[i][k]; k++) out[o++]=r_a[i][k];
        if (fields >= 2) { out[o++]='|'; for (int k=0; r_b[i][k]; k++) out[o++]=r_b[i][k]; }
        if (fields >= 3) { out[o++]='|'; for (int k=0; r_c[i][k]; k++) out[o++]=r_c[i][k]; }
        out[o++]='\n';
    }
    fs_write(path, out, (size_t)o);
}

/* ======================= CONTACTS ======================= */
void contacts_run(void) {
    rec_load("/home/contacts.txt", 3);
    int sel = 0; char query[40] = {0};
    for (;;) {
        vga_clear();
        ui_panel(2, 1, 76, 22, "Contacts", VGA_LCYAN, VGA_BLACK);
        if (query[0]) { char s[60]; int n=0; for (const char*p="search: ";*p;p++) s[n++]=*p; for (int k=0;query[k]&&n<58;k++) s[n++]=query[k]; s[n]=0; ui_text(5, 2, s, VGA_YELLOW, VGA_BLACK); }
        ui_text(5, 3, "Name", VGA_DGREY, VGA_BLACK); ui_text(38, 3, "Phone", VGA_DGREY, VGA_BLACK); ui_text(58, 3, "Email", VGA_DGREY, VGA_BLACK);
        int shown = 0;
        for (int i = 0; i < r_n; i++) {
            if (query[0] && !(ci_has(r_a[i], query) || ci_has(r_b[i], query) || ci_has(r_c[i], query))) continue;
            int y = 4 + shown; if (y > 20) break;
            uint8_t fg = (i==sel)?VGA_BLACK:VGA_LGREY, bg = (i==sel)?VGA_LGREY:VGA_BLACK;
            for (int x = 4; x <= 75; x++) vga_cell(x, y, ' ', fg, bg);
            int x = 5; for (int k=0;r_a[i][k]&&x<37;k++) vga_cell(x++,y,r_a[i][k],fg,bg);
            x = 38; for (int k=0;r_b[i][k]&&x<57;k++) vga_cell(x++,y,r_b[i][k],fg,bg);
            x = 58; for (int k=0;r_c[i][k]&&x<75;k++) vga_cell(x++,y,r_c[i][k],fg,bg);
            shown++;
        }
        vga_statusbar(" CONTACTS  Up/Dn  a add  e edit  d del  / search  c clear  q quit");
        vga_setcursor(0, 24);
        char k = keyboard_getc();
        if (k=='q'||k==27) { rec_save("/home/contacts.txt",3); break; }
        else if (k==0x10) { if (sel>0) sel--; }
        else if (k==0x0E) { if (sel<r_n-1) sel++; }
        else if (k=='a') { if (r_n<RMAX) { char b[RLEN]; if (t2_prompt(23,"Name: ",b,RLEN,0)>0) { strcpy(r_a[r_n],b); t2_prompt(23,"Phone: ",r_b[r_n],RLEN,0); t2_prompt(23,"Email: ",r_c[r_n],RLEN,0); sel=r_n; r_n++; rec_save("/home/contacts.txt",3); } } }
        else if (k=='e') { if (r_n) { t2_prompt(23,"Name: ",r_a[sel],RLEN,r_a[sel]); t2_prompt(23,"Phone: ",r_b[sel],RLEN,r_b[sel]); t2_prompt(23,"Email: ",r_c[sel],RLEN,r_c[sel]); rec_save("/home/contacts.txt",3); } }
        else if (k=='d') { if (r_n) { for (int i=sel;i<r_n-1;i++){strcpy(r_a[i],r_a[i+1]);strcpy(r_b[i],r_b[i+1]);strcpy(r_c[i],r_c[i+1]);} r_n--; if(sel>=r_n&&sel>0)sel--; rec_save("/home/contacts.txt",3); } }
        else if (k=='/') { t2_prompt(23,"Search: ",query,40,0); sel=0; }
        else if (k=='c') { query[0]=0; }
    }
    vga_clear(); vga_setcolor(VGA_LCYAN,VGA_BLACK); kputs("Contacts saved to /home/contacts.txt.\n"); vga_setcolor(VGA_LGREY,VGA_BLACK);
}

/* ======================= BOOKMARKS ======================= */
void bookmarks_run(void) {
    rec_load("/home/bookmarks.txt", 2);
    int sel = 0;
    for (;;) {
        vga_clear();
        ui_panel(2, 1, 76, 22, "Bookmarks", VGA_LCYAN, VGA_BLACK);
        if (!r_n) ui_text(6, 4, "No bookmarks. Press 'a' to add one.", VGA_DGREY, VGA_BLACK);
        for (int i = 0; i < r_n; i++) {
            int y = 3 + i; if (y > 20) break;
            uint8_t fg=(i==sel)?VGA_BLACK:VGA_LGREY, bg=(i==sel)?VGA_LGREY:VGA_BLACK;
            for (int x=4;x<=75;x++) vga_cell(x,y,' ',fg,bg);
            int x=5; for (int k=0;r_a[i][k]&&x<33;k++) vga_cell(x++,y,r_a[i][k],fg,bg);
            x=34; for (int k=0;r_b[i][k]&&x<75;k++) vga_cell(x++,y,r_b[i][k], (i==sel)?VGA_BLACK:VGA_LCYAN, bg);
        }
        vga_statusbar(" BOOKMARKS  Up/Dn  Enter open  a add  e edit  d del  q quit");
        vga_setcursor(0, 24);
        char k = keyboard_getc();
        if (k=='q'||k==27) { rec_save("/home/bookmarks.txt",2); break; }
        else if (k==0x10) { if (sel>0) sel--; }
        else if (k==0x0E) { if (sel<r_n-1) sel++; }
        else if (k=='\n') { if (r_n) browser_run(r_b[sel]); }
        else if (k=='a') { if (r_n<RMAX) { if (t2_prompt(23,"Name: ",r_a[r_n],RLEN,0)>0) { t2_prompt(23,"URL: ",r_b[r_n],RLEN,0); sel=r_n; r_n++; rec_save("/home/bookmarks.txt",2); } } }
        else if (k=='e') { if (r_n) { t2_prompt(23,"Name: ",r_a[sel],RLEN,r_a[sel]); t2_prompt(23,"URL: ",r_b[sel],RLEN,r_b[sel]); rec_save("/home/bookmarks.txt",2); } }
        else if (k=='d') { if (r_n) { for (int i=sel;i<r_n-1;i++){strcpy(r_a[i],r_a[i+1]);strcpy(r_b[i],r_b[i+1]);} r_n--; if(sel>=r_n&&sel>0)sel--; rec_save("/home/bookmarks.txt",2); } }
    }
    vga_clear(); vga_setcolor(VGA_LCYAN,VGA_BLACK); kputs("Bookmarks saved.\n"); vga_setcolor(VGA_LGREY,VGA_BLACK);
}

/* ======================= TIMER ======================= */
void timer_run(void) {
    int total = 300, remain = 300, running = 0; uint32_t last = pit_ticks();
    for (;;) {
        vga_clear();
        ui_panel(2, 1, 76, 22, "Timer", VGA_LCYAN, VGA_BLACK);
        int mm = remain/60, ss = remain%60;
        char tb[8]; tb[0]=(char)('0'+(mm/10)%10); tb[1]=(char)('0'+mm%10); tb[2]=':'; tb[3]=(char)('0'+ss/10); tb[4]=(char)('0'+ss%10); tb[5]=0;
        ui_box(28, 4, 20, 3, remain==0 ? VGA_LRED : VGA_DGREY, VGA_BLACK);
        ui_text(34, 5, tb, remain==0 ? VGA_LRED : VGA_WHITE, VGA_BLACK);
        ui_center(8, 2, 76, remain==0 ? "*** TIME ! ***" : running ? "running" : "paused", remain==0?VGA_LRED:running?VGA_LGREEN:VGA_DGREY, VGA_BLACK);
        vga_statusbar(" TIMER  space start/pause  [ ] -/+1min  -/+ 10s  r reset  q quit");
        vga_setcursor(0, 24);
        uint32_t s = pit_ticks(); int act = 0;
        while (pit_ticks() - s < 25) {
            if (running && remain > 0 && pit_ticks() - last >= 100) { remain--; last = pit_ticks(); act = 1; break; }
            char c = keyboard_trygetc();
            if (c) {
                if (c=='q'||c==27) { vga_clear(); vga_setcolor(VGA_LCYAN,VGA_BLACK); kputs("Timer closed.\n"); vga_setcolor(VGA_LGREY,VGA_BLACK); return; }
                if (c==' ') { running = !running; last = pit_ticks(); }
                else if (c=='r') { remain = total; running = 0; }
                else if (c=='[') { if (total>=60) { total-=60; remain=total; } }
                else if (c==']') { total+=60; remain=total; }
                else if (c=='-') { if (total>=10) { total-=10; remain=total; } }
                else if (c=='+'||c=='=') { total+=10; remain=total; }
                act = 1; break;
            }
            __asm__ volatile("hlt");
        }
        (void)act;
    }
}

/* ======================= COUNTER ======================= */
void counter_run(void) {
    rec_load("/home/counters.txt", 2);
    int sel = 0;
    for (;;) {
        vga_clear();
        ui_panel(2, 1, 76, 22, "Counters", VGA_LCYAN, VGA_BLACK);
        if (!r_n) ui_text(6, 4, "No counters. Press 'a' to add one.", VGA_DGREY, VGA_BLACK);
        for (int i = 0; i < r_n; i++) {
            int y = 3 + i; if (y > 20) break;
            uint8_t fg=(i==sel)?VGA_BLACK:VGA_LGREY, bg=(i==sel)?VGA_LGREY:VGA_BLACK;
            for (int x=4;x<=75;x++) vga_cell(x,y,' ',fg,bg);
            int x=5; for (int k=0;r_a[i][k]&&x<44;k++) vga_cell(x++,y,r_a[i][k],fg,bg);
            char num[16]; int v=(int)t2_atol(r_b[i]);
            int nn=0; int av=v<0?-v:v; char tmp[12]; int tn=0; if(av==0)tmp[tn++]='0'; while(av){tmp[tn++]=(char)('0'+av%10);av/=10;} if(v<0)num[nn++]='-'; while(tn)num[nn++]=tmp[--tn]; num[nn]=0;
            x=48; for (int k=0;num[k];k++) vga_cell(x++,y,num[k], (i==sel)?VGA_BLACK:VGA_LCYAN, bg);
        }
        vga_statusbar(" COUNTERS  Up/Dn  +/- adjust  a add  d del  r reset  q quit");
        vga_setcursor(0, 24);
        char k = keyboard_getc();
        if (k=='q'||k==27) { rec_save("/home/counters.txt",2); break; }
        else if (k==0x10) { if (sel>0) sel--; }
        else if (k==0x0E) { if (sel<r_n-1) sel++; }
        else if (k=='+'||k=='=') { if (r_n) { int v=(int)t2_atol(r_b[sel])+1; kprintf(""); char b[12]; int n=0,av=v<0?-v:v,t=0; char tt[12]; if(av==0)tt[t++]='0'; while(av){tt[t++]=(char)('0'+av%10);av/=10;} if(v<0)b[n++]='-'; while(t)b[n++]=tt[--t]; b[n]=0; strcpy(r_b[sel],b); rec_save("/home/counters.txt",2); } }
        else if (k=='-'||k=='_') { if (r_n) { int v=(int)t2_atol(r_b[sel])-1; char b[12]; int n=0,av=v<0?-v:v,t=0; char tt[12]; if(av==0)tt[t++]='0'; while(av){tt[t++]=(char)('0'+av%10);av/=10;} if(v<0)b[n++]='-'; while(t)b[n++]=tt[--t]; b[n]=0; strcpy(r_b[sel],b); rec_save("/home/counters.txt",2); } }
        else if (k=='r') { if (r_n) { strcpy(r_b[sel],"0"); rec_save("/home/counters.txt",2); } }
        else if (k=='a') { if (r_n<RMAX) { if (t2_prompt(23,"Counter name: ",r_a[r_n],RLEN,0)>0) { strcpy(r_b[r_n],"0"); sel=r_n; r_n++; rec_save("/home/counters.txt",2); } } }
        else if (k=='d') { if (r_n) { for (int i=sel;i<r_n-1;i++){strcpy(r_a[i],r_a[i+1]);strcpy(r_b[i],r_b[i+1]);} r_n--; if(sel>=r_n&&sel>0)sel--; rec_save("/home/counters.txt",2); } }
    }
    vga_clear(); vga_setcolor(VGA_LCYAN,VGA_BLACK); kputs("Counters saved.\n"); vga_setcolor(VGA_LGREY,VGA_BLACK);
}

/* ======================= EXPENSES ======================= */
void expenses_run(void) {
    rec_load("/home/expenses.txt", 2);
    int sel = 0;
    for (;;) {
        long total = 0; for (int i=0;i<r_n;i++) total += t2_atol(r_b[i]);
        vga_clear();
        ui_panel(2, 1, 76, 22, "Expenses", VGA_LCYAN, VGA_BLACK);
        { char s[40]; int n=0; for (const char*p="Total: ";*p;p++) s[n++]=*p; long tv=total<0?-total:total; char tmp[12]; int tn=0; if(!tv)tmp[tn++]='0'; while(tv){tmp[tn++]=(char)('0'+tv%10);tv/=10;} if(total<0)s[n++]='-'; while(tn)s[n++]=tmp[--tn]; s[n]=0; ui_text(5, 2, s, VGA_LGREEN, VGA_BLACK); }
        if (!r_n) ui_text(6, 4, "No expenses. Press 'a' to add one.", VGA_DGREY, VGA_BLACK);
        for (int i = 0; i < r_n; i++) {
            int y = 4 + i; if (y > 20) break;
            uint8_t fg=(i==sel)?VGA_BLACK:VGA_LGREY, bg=(i==sel)?VGA_LGREY:VGA_BLACK;
            for (int x=4;x<=75;x++) vga_cell(x,y,' ',fg,bg);
            int x=5; for (int k=0;r_a[i][k]&&x<52;k++) vga_cell(x++,y,r_a[i][k],fg,bg);
            x=54; for (int k=0;r_b[i][k]&&x<75;k++) vga_cell(x++,y,r_b[i][k],(i==sel)?VGA_BLACK:VGA_LGREEN,bg);
        }
        vga_statusbar(" EXPENSES  Up/Dn  a add  e edit  d del  q quit");
        vga_setcursor(0, 24);
        char k = keyboard_getc();
        if (k=='q'||k==27) { rec_save("/home/expenses.txt",2); break; }
        else if (k==0x10) { if (sel>0) sel--; }
        else if (k==0x0E) { if (sel<r_n-1) sel++; }
        else if (k=='a') { if (r_n<RMAX) { if (t2_prompt(23,"Label: ",r_a[r_n],RLEN,0)>0) { t2_prompt(23,"Amount: ",r_b[r_n],RLEN,0); sel=r_n; r_n++; rec_save("/home/expenses.txt",2); } } }
        else if (k=='e') { if (r_n) { t2_prompt(23,"Label: ",r_a[sel],RLEN,r_a[sel]); t2_prompt(23,"Amount: ",r_b[sel],RLEN,r_b[sel]); rec_save("/home/expenses.txt",2); } }
        else if (k=='d') { if (r_n) { for (int i=sel;i<r_n-1;i++){strcpy(r_a[i],r_a[i+1]);strcpy(r_b[i],r_b[i+1]);} r_n--; if(sel>=r_n&&sel>0)sel--; rec_save("/home/expenses.txt",2); } }
    }
    vga_clear(); vga_setcolor(VGA_LCYAN,VGA_BLACK); kputs("Expenses saved.\n"); vga_setcolor(VGA_LGREY,VGA_BLACK);
}
