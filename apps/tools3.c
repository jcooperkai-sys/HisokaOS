/* tools3.c - a third batch of real boxed utility apps (no games), features-for-features. */
#include "tools3.h"
#include "vga.h"
#include "keyboard.h"
#include "printf.h"
#include "ramfs.h"
#include "string.h"
#include "pit.h"
#include "rtc.h"
#include "net.h"
#include "rtl8139.h"
#include "ui.h"
#include "types.h"

/* ---- shared ---- */
static int t3_prompt(int x, int y, char *buf, int max) {
    int len = 0; buf[0] = 0; vga_setcursor(x, y);
    for (;;) {
        char c = keyboard_getc();
        if (c=='\n'||c=='\r') { buf[len]=0; return len; }
        else if (c==27) { buf[0]=0; return -1; }
        else if (c=='\b') { if (len) { len--; vga_cell(x+len,y,' ',VGA_WHITE,VGA_BLACK); vga_setcursor(x+len,y); } }
        else if (c>=32&&c<127&&len<max-1) { buf[len++]=c; vga_cell(x+len-1,y,c,VGA_WHITE,VGA_BLACK); vga_setcursor(x+len,y); }
    }
}
static void put2(int x, int y, int v, uint8_t fg) { vga_cell(x, y, (char)('0'+(v/10)%10), fg, VGA_BLACK); vga_cell(x+1, y, (char)('0'+v%10), fg, VGA_BLACK); }

/* ======================= WORLD CLOCK ======================= */
void worldclock_run(void) {
    static const char *zone[] = { "UTC      ", "New York ", "Chicago  ", "Denver   ", "Los Angeles",
                                  "London   ", "Paris    ", "India    ", "Tokyo    ", "Sydney   " };
    static const int off[] = { 0, -5, -6, -7, -8, 0, 1, 5, 9, 10 };  /* hours from UTC (approx, no DST) */
    for (;;) {
        rtc_time_t t; rtc_now(&t);
        vga_clear();
        ui_panel(2, 1, 76, 22, "World Clock", VGA_LCYAN, VGA_BLACK);
        ui_text(6, 3, "City            Time", VGA_DGREY, VGA_BLACK);
        for (int i = 0; i < 10; i++) {
            int y = 5 + i;
            ui_text(6, y, zone[i], VGA_LGREY, VGA_BLACK);
            int h = ((int)t.hour + off[i] + 24) % 24;
            put2(24, y, h, VGA_LGREEN); vga_cell(26, y, ':', VGA_LGREEN, VGA_BLACK); put2(27, y, t.min, VGA_LGREEN);
            vga_cell(30, y, ':', VGA_DGREY, VGA_BLACK); put2(31, y, t.sec, VGA_DGREY);
        }
        vga_statusbar(" WORLD CLOCK   live   q quit");
        vga_setcursor(0, 24);
        uint32_t s = pit_ticks();
        while (pit_ticks() - s < 50) { char c = keyboard_trygetc(); if (c=='q'||c==27) { vga_clear(); return; } __asm__ volatile("hlt"); }
    }
}

/* ======================= NET TOOLS ======================= */
void nettools_run(void) {
    char status[60] = "type a host and press Enter to resolve + ping it";
    uint8_t scol = VGA_DGREY;
    for (;;) {
        vga_clear();
        ui_panel(2, 1, 76, 22, "Net Tools", VGA_LCYAN, VGA_BLACK);
        const uint8_t *ip = net_my_ip(), *gw = net_gw_ip();
        char l[48]; int n;
        n = 0; for (const char*p="IP      : ";*p;p++) l[n++]=*p; for (int o=0;o<4;o++){ uint32_t v=ip[o]; char t[4]; int m=0; if(!v)t[m++]='0'; while(v){t[m++]=(char)('0'+v%10);v/=10;} while(m)l[n++]=t[--m]; if(o<3)l[n++]='.'; } l[n]=0; ui_text(5,3,l,VGA_LGREY,VGA_BLACK);
        n = 0; for (const char*p="Gateway : ";*p;p++) l[n++]=*p; for (int o=0;o<4;o++){ uint32_t v=gw[o]; char t[4]; int m=0; if(!v)t[m++]='0'; while(v){t[m++]=(char)('0'+v%10);v/=10;} while(m)l[n++]=t[--m]; if(o<3)l[n++]='.'; } l[n]=0; ui_text(5,4,l,VGA_LGREY,VGA_BLACK);
        ui_text(5, 5, rtl8139_present() ? "Adapter : rtl8139  (online)" : "Adapter : none  (offline)", rtl8139_present()?VGA_LGREEN:VGA_YELLOW, VGA_BLACK);
        ui_text(5, 8, "host >", VGA_LMAGENTA, VGA_BLACK);
        ui_text(5, 11, status, scol, VGA_BLACK);
        vga_statusbar(" NET TOOLS   type host + Enter to test   q quit");
        char host[48];
        int r = t3_prompt(12, 8, host, 48);
        if (r < 0) { vga_clear(); return; }
        if (r == 1 && (host[0]=='q'||host[0]=='Q')) { vga_clear(); return; }
        if (r == 0) continue;
        uint8_t rip[4];
        if (!net_dns_resolve(host, rip)) { strcpy(status, "DNS: could not resolve that host"); scol = VGA_LRED; continue; }
        int ttl = 0; int ok = net_ping(rip, &ttl);
        int sn = 0; for (const char*p=host;*p&&sn<20;p++) status[sn++]=*p; status[sn++]=' '; status[sn++]='=';status[sn++]='>';status[sn++]=' ';
        for (int o=0;o<4;o++){ uint32_t v=rip[o]; char t[4]; int m=0; if(!v)t[m++]='0'; while(v){t[m++]=(char)('0'+v%10);v/=10;} while(m)status[sn++]=t[--m]; if(o<3)status[sn++]='.'; }
        const char *res = ok ? "  reply OK" : "  no reply"; for (const char*p=res;*p;p++)status[sn++]=*p; status[sn]=0;
        scol = ok ? VGA_LGREEN : VGA_YELLOW;
    }
}

/* ======================= DISK USAGE ======================= */
void diskusage_run(void) {
    static const char *top[] = { "/home", "/System", "/etc", "/usr", "/var", "/dev", "/proc", "/sys", "/Applications", "/bin", "/tmp", "/root" };
    const int N = 12;
    for (;;) {
        int cnt[12] = {0}; int total = 0;
        for (int i = 0; i < FS_MAX_FILES; i++) {
            fs_file_t *f = fs_at(i); if (!f || !f->used || f->is_dir) continue;
            total++;
            for (int k = 0; k < N; k++) { int tl = (int)strlen(top[k]); if (!strncmp(f->name, top[k], (size_t)tl) && (f->name[tl]=='/')) { cnt[k]++; break; } }
        }
        int mx = 1; for (int k = 0; k < N; k++) if (cnt[k] > mx) mx = cnt[k];
        vga_clear();
        ui_panel(2, 1, 76, 22, "Disk Usage", VGA_LCYAN, VGA_BLACK);
        ui_text(5, 3, "Files by top-level folder", VGA_DGREY, VGA_BLACK);
        for (int k = 0; k < N; k++) {
            int y = 5 + k;
            ui_text(5, y, top[k], VGA_LGREY, VGA_BLACK);
            int bw = cnt[k] * 30 / mx; for (int x = 0; x < 30; x++) vga_cell(22 + x, y, ' ', VGA_WHITE, x < bw ? VGA_LGREEN : VGA_BLACK);
            put2(55, y, cnt[k] > 99 ? 99 : cnt[k], VGA_LCYAN);
        }
        char tl[32]; int n = 0; for (const char*p="total files: ";*p;p++) tl[n++]=*p; { int v=total; char t[6]; int m=0; if(!v)t[m++]='0'; while(v){t[m++]=(char)('0'+v%10);v/=10;} while(m)tl[n++]=t[--m]; } tl[n]=0;
        ui_text(5, 19, tl, VGA_WHITE, VGA_BLACK);
        vga_statusbar(" DISK USAGE   q quit");
        vga_setcursor(0, 24);
        char k = keyboard_getc(); if (k=='q'||k==27) { vga_clear(); return; }
    }
}

/* ======================= shared record store for Kanban + Notes ======================= */
#define K_MAX 64
#define K_LEN 72
static char kt[K_MAX][K_LEN]; static int kc[K_MAX]; static int kn;
static void k_load(const char *path, int cols) {
    kn = 0; fs_file_t *f = fs_find(path); if (!f || f->is_dir) return;
    char line[K_LEN+4]; int li = 0;
    for (size_t i = 0; i <= f->len && kn < K_MAX; i++) {
        char c = (i < f->len) ? (char)f->data[i] : '\n';
        if (c == '\n') {
            line[li] = 0;
            if (li > 0) {
                int col = 0, off = 0;
                if (cols && line[0] >= '0' && line[0] <= '2' && line[1] == '|') { col = line[0]-'0'; off = 2; }
                strncpy(kt[kn], line+off, K_LEN-1); kt[kn][K_LEN-1]=0; kc[kn]=col; kn++;
            }
            li = 0;
        } else if (li < K_LEN+2) line[li++] = c;
    }
}
static void k_save(const char *path, int cols) {
    static char out[K_MAX*(K_LEN+3)]; int o = 0;
    for (int i = 0; i < kn; i++) { if (cols) { out[o++]=(char)('0'+kc[i]); out[o++]='|'; } for (int k=0;kt[i][k];k++) out[o++]=kt[i][k]; out[o++]='\n'; }
    fs_write(path, out, (size_t)o);
}

/* ======================= KANBAN ======================= */
void kanban_run(void) {
    k_load("/home/kanban.txt", 1);
    static const char *colname[] = { "TODO", "DOING", "DONE" };
    static const int colx[] = { 4, 29, 54 };
    int sel = 0;
    for (;;) {
        vga_clear();
        ui_panel(2, 1, 76, 22, "Kanban Board", VGA_LCYAN, VGA_BLACK);
        for (int c = 0; c < 3; c++) {
            ui_text(colx[c]+1, 3, colname[c], VGA_YELLOW, VGA_BLACK);
            for (int x = colx[c]; x < colx[c]+22; x++) vga_cell(x, 4, (char)0xC4, VGA_DGREY, VGA_BLACK);
        }
        int rowc[3] = {0,0,0};
        for (int i = 0; i < kn; i++) {
            int c = kc[i], y = 5 + rowc[c]; if (y > 20) { rowc[c]++; continue; }
            uint8_t fg = (i==sel)?VGA_BLACK:VGA_LGREY, bg = (i==sel)?VGA_LGREY:VGA_BLACK;
            for (int x = colx[c]; x < colx[c]+22; x++) vga_cell(x, y, ' ', fg, bg);
            int x = colx[c]+1; for (int k=0;kt[i][k]&&x<colx[c]+21;k++) vga_cell(x++, y, kt[i][k], fg, bg);
            rowc[c]++;
        }
        vga_statusbar(" KANBAN  Up/Dn select  [ ] move column  a add  d del  q quit");
        vga_setcursor(0, 24);
        char k = keyboard_getc();
        if (k=='q'||k==27) { k_save("/home/kanban.txt",1); break; }
        else if (k==0x10) { if (sel>0) sel--; }
        else if (k==0x0E) { if (sel<kn-1) sel++; }
        else if (k=='[') { if (kn && kc[sel]>0) { kc[sel]--; k_save("/home/kanban.txt",1); } }
        else if (k==']') { if (kn && kc[sel]<2) { kc[sel]++; k_save("/home/kanban.txt",1); } }
        else if (k=='a') { if (kn<K_MAX) { for (int x=4;x<=75;x++) vga_cell(x,23,' ',VGA_WHITE,VGA_BLACK); ui_text(4,23,"New card: ",VGA_YELLOW,VGA_BLACK); char b[K_LEN]; if (t3_prompt(14,23,b,K_LEN)>0) { strcpy(kt[kn],b); kc[kn]=0; sel=kn; kn++; k_save("/home/kanban.txt",1); } } }
        else if (k=='d') { if (kn) { for (int i=sel;i<kn-1;i++){strcpy(kt[i],kt[i+1]);kc[i]=kc[i+1];} kn--; if(sel>=kn&&sel>0)sel--; k_save("/home/kanban.txt",1); } }
    }
    vga_clear(); vga_setcolor(VGA_LCYAN,VGA_BLACK); kputs("Kanban saved.\n"); vga_setcolor(VGA_LGREY,VGA_BLACK);
}

/* ======================= NOTES ======================= */
void notes_run(void) {
    k_load("/home/notes.txt", 0);
    int sel = 0;
    for (;;) {
        vga_clear();
        ui_panel(2, 1, 76, 22, "Notes", VGA_LCYAN, VGA_BLACK);
        if (!kn) ui_text(6, 4, "No notes yet. Press 'a' to add one.", VGA_DGREY, VGA_BLACK);
        for (int i = 0; i < kn; i++) {
            int y = 3 + i; if (y > 20) break;
            uint8_t fg=(i==sel)?VGA_BLACK:VGA_LGREY, bg=(i==sel)?VGA_LGREY:VGA_BLACK;
            for (int x=4;x<=75;x++) vga_cell(x,y,' ',fg,bg);
            vga_cell(5,y,(char)0x07,fg,bg);  /* a bullet */
            int x=7; for (int k=0;kt[i][k]&&x<75;k++) vga_cell(x++,y,kt[i][k],fg,bg);
        }
        vga_statusbar(" NOTES  Up/Dn  a add  e edit  d del  q quit");
        vga_setcursor(0, 24);
        char k = keyboard_getc();
        if (k=='q'||k==27) { k_save("/home/notes.txt",0); break; }
        else if (k==0x10) { if (sel>0) sel--; }
        else if (k==0x0E) { if (sel<kn-1) sel++; }
        else if (k=='a') { if (kn<K_MAX) { for (int x=4;x<=75;x++) vga_cell(x,23,' ',VGA_WHITE,VGA_BLACK); ui_text(4,23,"Note: ",VGA_YELLOW,VGA_BLACK); char b[K_LEN]; if (t3_prompt(10,23,b,K_LEN)>0) { strcpy(kt[kn],b); kc[kn]=0; sel=kn; kn++; k_save("/home/notes.txt",0); } } }
        else if (k=='e') { if (kn) { for (int x=4;x<=75;x++) vga_cell(x,23,' ',VGA_WHITE,VGA_BLACK); ui_text(4,23,"Edit: ",VGA_YELLOW,VGA_BLACK); int px=10; for (int j=0;kt[sel][j];j++) vga_cell(px+j,23,kt[sel][j],VGA_WHITE,VGA_BLACK); char b[K_LEN]; if (t3_prompt(10+ (int)strlen(kt[sel]),23,b,K_LEN)>=0) { /* simple: append edit */ } char nb[K_LEN]; for (int x=4;x<=75;x++) vga_cell(x,23,' ',VGA_WHITE,VGA_BLACK); ui_text(4,23,"Note: ",VGA_YELLOW,VGA_BLACK); if (t3_prompt(10,23,nb,K_LEN)>0){ strcpy(kt[sel],nb); k_save("/home/notes.txt",0);} } }
        else if (k=='d') { if (kn) { for (int i=sel;i<kn-1;i++){strcpy(kt[i],kt[i+1]);} kn--; if(sel>=kn&&sel>0)sel--; k_save("/home/notes.txt",0); } }
    }
    vga_clear(); vga_setcolor(VGA_LCYAN,VGA_BLACK); kputs("Notes saved.\n"); vga_setcolor(VGA_LGREY,VGA_BLACK);
}
