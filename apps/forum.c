/* forum.c - the Hisoka Forum: computers talking to computers over the network. The
 * model runs on the host (forum-server.py); this is the native client. Tile/square
 * UI you can click with the mouse. Genres, posts, comments, fuzzy search, DMs, support. */
#include "forum.h"
#include "vga.h"
#include "keyboard.h"
#include "mouse.h"
#include "net.h"
#include "rtl8139.h"
#include "ramfs.h"
#include "ui.h"
#include "printf.h"
#include "string.h"
#include "types.h"

#define FHOST "10.0.2.2"
#define FPORT 8092
static char fbuf[16384];
static char me[40];

static void get_host(void) {
    fs_file_t *f = fs_find("/etc/hostname"); int o = 0;
    if (f && f->data) for (size_t i = 0; i < f->len && o < 38; i++) { char c = (char)f->data[i]; if (c=='\n') break; me[o++] = c; }
    if (!o) { const char *d = "hisoka"; for (int i = 0; d[i]; i++) me[o++] = d[i]; }
    me[o] = 0;
}
static void url_enc(const char *s, char *o, int max) {
    const char *h = "0123456789ABCDEF"; int k = 0;
    for (; *s && k < max-4; s++) { char c = *s;
        if ((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.') o[k++]=c;
        else if (c==' ') o[k++]='+';
        else { o[k++]='%'; o[k++]=h[(uint8_t)c>>4]; o[k++]=h[(uint8_t)c&15]; } }
    o[k] = 0;
}
/* GET path; move the body to the front of fbuf, return its length (-1 on error) */
static int forum_fetch(const char *path) {
    int n = net_http_get_ep(FHOST, FPORT, path, fbuf, sizeof(fbuf));
    if (n <= 0) return -1;
    for (int i = 0; i + 3 < n; i++)
        if (fbuf[i]=='\r'&&fbuf[i+1]=='\n'&&fbuf[i+2]=='\r'&&fbuf[i+3]=='\n') {
            int bs = i+4, bl = n - bs; memmove(fbuf, fbuf + bs, (size_t)bl); fbuf[bl] = 0; return bl;
        }
    return -1;
}
/* a boxed text prompt at (x,y); returns len, -1 on Esc */
static int finput(int x, int y, char *b, int max) {
    int len = 0; b[0] = 0; vga_setcursor(x, y);
    for (;;) { char c = keyboard_getc();
        if (c=='\n'||c=='\r') { b[len]=0; return len; }
        else if (c==27) { b[0]=0; return -1; }
        else if (c=='\b') { if (len) { len--; vga_cell(x+len,y,' ',VGA_WHITE,VGA_BLACK); vga_setcursor(x+len,y); } }
        else if (c>=32&&c<127&&len<max-1&&x+len<76) { b[len++]=c; vga_cell(x+len-1,y,c,VGA_WHITE,VGA_BLACK); vga_setcursor(x+len,y); } }
}

/* a selectable list (keyboard + mouse), rows start at y0; returns index or -1 */
static int list_pick(const char *title, char rows[][96], int n, int y0) {
    int sel = 0, top = 0, pbtn = 0; const int VIS = 22 - y0;
    for (;;) {
        if (sel < top) top = sel; if (sel >= top+VIS) top = sel - VIS + 1;
        vga_clear(); ui_panel(2, 1, 76, 22, title, VGA_LCYAN, VGA_BLACK);
        if (!n) ui_text(6, y0+1, "(nothing here yet)", VGA_DGREY, VGA_BLACK);
        for (int r = 0; r < VIS; r++) { int i = top+r; if (i>=n) break; int y = y0+r;
            uint8_t fg=(i==sel)?VGA_BLACK:VGA_LGREY, bg=(i==sel)?VGA_LGREY:VGA_BLACK;
            for (int x=4;x<=72;x++) vga_cell(x,y,' ',fg,bg);
            for (int k=0; rows[i][k] && k<67; k++) vga_cell(5+k,y,rows[i][k],fg,bg); }
        vga_statusbar(" Up/Dn or mouse   Enter/click open   q back");
        vga_setcursor(0,24);
        for (;;) {
            char c = keyboard_trygetc();
            if (c=='q'||c==27) return -1;
            else if (c=='n') return -2;   /* new post */
            else if (c==0x10) { if (sel>0) sel--; break; }
            else if (c==0x0E) { if (sel<n-1) sel++; break; }
            else if (c=='\n') return n? sel : -1;
            if (mouse_present()) { int mx,my,b; mouse_get(&mx,&my,&b);
                int r=my-y0, idx=(r>=0&&r<VIS&&mx>=4&&mx<=72)? top+r : -1; if (idx>=n) idx=-1;
                if ((b&1)&&!(pbtn&1)) { pbtn=b; if (idx>=0) return idx; break; }
                pbtn=b; if (idx>=0&&idx!=sel) { sel=idx; break; } }
            __asm__ volatile("hlt");
        }
    }
}

/* split fbuf (already body) into rows[]; returns count. keeps the id (first tab field) */
static int parse_posts(char rows[][96], char ids[][8], int max) {
    int n = 0, i = 0;
    while (fbuf[i] && n < max) {
        int s = i; while (fbuf[i] && fbuf[i] != '\n') i++;
        /* line: id \t title \t author \t ncomments \t pin */
        int f0 = s; while (f0 < i && fbuf[f0] != '\t') f0++;
        int idn = f0 - s; if (idn > 6) idn = 6;
        for (int k = 0; k < idn; k++) ids[n][k] = fbuf[s+k]; ids[n][idn] = 0;
        /* build a display row: title (author) [ncomments] */
        int o = 0; rows[n][o++]='['; for (int k=0;k<idn&&o<6;k++) rows[n][o++]=fbuf[s+k]; rows[n][o++]=']'; rows[n][o++]=' ';
        int p = f0+1, fld = 1;
        for (; p < i && o < 94; p++) { if (fbuf[p]=='\t') { if (fld==1){ rows[n][o++]=' ';rows[n][o++]='-';rows[n][o++]=' '; } if (fld>=2) break; fld++; } else rows[n][o++]=fbuf[p]; }
        rows[n][o]=0;
        n++; if (fbuf[i]=='\n') i++;
    }
    return n;
}

static void read_post(const char *id) {
    char path[64]; int o=0; for(const char*p="/forum/post?id=";*p;p++)path[o++]=*p; for(const char*p=id;*p;p++)path[o++]=*p; path[o]=0;
    if (forum_fetch(path) < 0) { vga_clear(); ui_panel(2,1,76,22,"Post",VGA_LCYAN,VGA_BLACK); ui_text(5,3,"could not reach the forum server",VGA_LRED,VGA_BLACK); vga_statusbar(" any key"); keyboard_getc(); return; }
    for (;;) {
        vga_clear(); ui_panel(2,1,76,22,"Post",VGA_LCYAN,VGA_BLACK);
        /* line 1: title \t author */
        int i=0; int t=i; while(fbuf[i]&&fbuf[i]!='\t')i++; char title[68]; int tl=i-t; if(tl>66)tl=66; for(int k=0;k<tl;k++)title[k]=fbuf[t+k]; title[tl]=0; if(fbuf[i]=='\t')i++;
        int a=i; while(fbuf[i]&&fbuf[i]!='\n')i++; char auth[24]; int al=i-a; if(al>22)al=22; for(int k=0;k<al;k++)auth[k]=fbuf[a+k]; auth[al]=0; if(fbuf[i]=='\n')i++;
        ui_text(5,3,title,VGA_WHITE,VGA_BLACK); ui_text(5,4,"by ",VGA_DGREY,VGA_BLACK); ui_text(8,4,auth,VGA_LCYAN,VGA_BLACK);
        /* body until <<COMMENTS>> */
        int x=5,y=6;
        while (fbuf[i] && strncmp(fbuf+i, "<<COMMENTS>>", 12)) { char c=fbuf[i++]; if(c=='\n'||x>74){x=5;y++;} if(c!='\n'&&y<14) vga_cell(x++,y,c,VGA_LGREY,VGA_BLACK); }
        if (!strncmp(fbuf+i,"<<COMMENTS>>",12)) i+=12; if(fbuf[i]=='\n')i++;
        ui_text(5,15,"Comments:",VGA_YELLOW,VGA_BLACK);
        int cy=16;
        while (fbuf[i] && cy<21) {
            int ca=i; while(fbuf[i]&&fbuf[i]!='\t')i++; char cau[20]; int cal=i-ca; if(cal>18)cal=18; for(int k=0;k<cal;k++)cau[k]=fbuf[ca+k]; cau[cal]=0; if(fbuf[i]=='\t')i++;
            int cb=i; while(fbuf[i]&&fbuf[i]!='\t'&&fbuf[i]!='\n')i++; char cbody[60]; int cbl=i-cb; if(cbl>56)cbl=56; for(int k=0;k<cbl;k++)cbody[k]=fbuf[cb+k]; cbody[cbl]=0;
            while(fbuf[i]&&fbuf[i]!='\n')i++; if(fbuf[i]=='\n')i++;
            ui_text(6,cy,cau,VGA_LCYAN,VGA_BLACK); ui_text(6+cal+1,cy,cbody,VGA_LGREY,VGA_BLACK); cy++;
        }
        vga_statusbar(" POST   c comment   q back"); vga_setcursor(0,24);
        char k = keyboard_getc();
        if (k=='q'||k==27) return;
        if (k=='c') {
            for(int xx=4;xx<=72;xx++) vga_cell(xx,23,' ',VGA_WHITE,VGA_BLACK);
            ui_text(4,23,"Comment: ",VGA_YELLOW,VGA_BLACK); char cm[120]; if(finput(13,23,cm,120)>0){ char enc[200],cp[300]; url_enc(cm,enc,sizeof enc); int oo=0; for(const char*p="/forum/comment?id=";*p;p++)cp[oo++]=*p; for(const char*p=id;*p;p++)cp[oo++]=*p; for(const char*p="&author=";*p;p++)cp[oo++]=*p; char me_e[60]; url_enc(me,me_e,sizeof me_e); for(const char*p=me_e;*p;p++)cp[oo++]=*p; for(const char*p="&body=";*p;p++)cp[oo++]=*p; for(const char*p=enc;*p&&oo<290;p++)cp[oo++]=*p; cp[oo]=0; forum_fetch(cp); forum_fetch(path); /* reload */ }
        }
    }
}

static void new_post(const char *genre) {
    vga_clear(); ui_panel(2,1,76,22,"New Post",VGA_LCYAN,VGA_BLACK);
    ui_text(5,3,"Genre :",VGA_LGREY,VGA_BLACK); ui_text(13,3,genre,VGA_LCYAN,VGA_BLACK);
    ui_text(5,5,"Title :",VGA_LGREY,VGA_BLACK); char title[80]; if(finput(13,5,title,80)<=0) return;
    ui_text(5,7,"Body  :",VGA_LGREY,VGA_BLACK); char body[200]; if(finput(13,7,body,200)<0) return;
    char ge[40],te[160],be[400],path[700]; url_enc(genre,ge,sizeof ge); url_enc(title,te,sizeof te); url_enc(body,be,sizeof be); char me_e[60]; url_enc(me,me_e,sizeof me_e);
    int o=0; for(const char*p="/forum/new?genre=";*p;p++)path[o++]=*p; for(const char*p=ge;*p;p++)path[o++]=*p;
    for(const char*p="&author=";*p;p++)path[o++]=*p; for(const char*p=me_e;*p;p++)path[o++]=*p;
    for(const char*p="&title=";*p;p++)path[o++]=*p; for(const char*p=te;*p;p++)path[o++]=*p;
    for(const char*p="&body=";*p;p++)path[o++]=*p; for(const char*p=be;*p&&o<690;p++)path[o++]=*p; path[o]=0;
    forum_fetch(path);
    ui_text(5,10,"Posted!",VGA_LGREEN,VGA_BLACK); vga_statusbar(" any key"); keyboard_getc();
}

static void posts_screen(const char *genre) {
    for (;;) {
        char path[64]; int o=0; for(const char*p="/forum/posts?genre=";*p;p++)path[o++]=*p; for(const char*p=genre;*p;p++)path[o++]=*p; path[o]=0;
        if (forum_fetch(path) < 0) { vga_clear(); ui_panel(2,1,76,22,genre,VGA_LCYAN,VGA_BLACK); ui_text(5,3,"could not reach the forum server",VGA_LRED,VGA_BLACK); ui_text(5,4,"start it on the host: python3 forum-server.py",VGA_DGREY,VGA_BLACK); vga_statusbar(" any key"); keyboard_getc(); return; }
        static char rows[40][96], ids[40][8];
        int n = parse_posts(rows, ids, 40);
        vga_statusbar(" POSTS  Enter/click read   n new post   q back");
        int sel = list_pick(genre, rows, n, 4);
        if (sel == -2) { new_post(genre); continue; }   /* 'n' = compose */
        if (sel < 0) return;
        read_post(ids[sel]);
    }
}

static void search_screen(void) {
    vga_clear(); ui_panel(2,1,76,22,"Search",VGA_LCYAN,VGA_BLACK);
    ui_text(5,3,"Search (typos ok):",VGA_LGREY,VGA_BLACK);
    char q[80]; if (finput(24,3,q,80)<=0) return;
    char qe[160], path[200]; url_enc(q,qe,sizeof qe); int o=0; for(const char*p="/forum/search?q=";*p;p++)path[o++]=*p; for(const char*p=qe;*p;p++)path[o++]=*p; path[o]=0;
    if (forum_fetch(path) < 0) return;
    static char rows[40][96], ids[40][8]; int n = parse_posts(rows, ids, 40);
    int sel = list_pick("Search results", rows, n, 4);
    if (sel >= 0) read_post(ids[sel]);
}

static void inbox_screen(void) {
    for (;;) {
        char path[64]; int o=0; for(const char*p="/dm/inbox?user=";*p;p++)path[o++]=*p; char me_e[60]; url_enc(me,me_e,sizeof me_e); for(const char*p=me_e;*p;p++)path[o++]=*p; path[o]=0;
        if (forum_fetch(path) < 0) { vga_clear(); ui_panel(2,1,76,22,"Inbox",VGA_LCYAN,VGA_BLACK); ui_text(5,3,"could not reach the forum server",VGA_LRED,VGA_BLACK); vga_statusbar(" any key"); keyboard_getc(); return; }
        static char rows[40][96]; int n=0, i=0;
        while (fbuf[i] && n<40) { int o2=0; rows[n][o2++]='<'; int a=i; while(fbuf[i]&&fbuf[i]!='\t')i++; for(int k=a;k<i&&o2<20;k++) rows[n][o2++]=fbuf[k]; rows[n][o2++]='>'; rows[n][o2++]=' '; if(fbuf[i]=='\t')i++; while(fbuf[i]&&fbuf[i]!='\n'&&o2<94) rows[n][o2++]=fbuf[i++]; rows[n][o2]=0; if(fbuf[i]=='\n')i++; n++; }
        vga_statusbar(" INBOX   s send a DM   q back");
        /* reuse list just to show; but we want 's' key. Do a simple loop */
        vga_clear(); ui_panel(2,1,76,22,"Inbox - messages to you",VGA_LCYAN,VGA_BLACK);
        if (!n) ui_text(6,4,"No messages.",VGA_DGREY,VGA_BLACK);
        for (int r=0;r<n&&r<16;r++) ui_text(5,4+r,rows[r],VGA_LGREY,VGA_BLACK);
        vga_statusbar(" INBOX   s send a DM   q back"); vga_setcursor(0,24);
        char k = keyboard_getc();
        if (k=='q'||k==27) return;
        if (k=='s') {
            vga_clear(); ui_panel(2,1,76,22,"Send a DM",VGA_LCYAN,VGA_BLACK);
            ui_text(5,3,"To (computer name):",VGA_LGREY,VGA_BLACK); char to[40]; if(finput(25,3,to,40)<=0) continue;
            ui_text(5,5,"Message:",VGA_LGREY,VGA_BLACK); char msg[160]; if(finput(14,5,msg,160)<=0) continue;
            char te[60],be[300],me_e[60],pp[500]; url_enc(to,te,sizeof te); url_enc(msg,be,sizeof be); url_enc(me,me_e,sizeof me_e);
            int oo=0; for(const char*p="/dm/send?from=";*p;p++)pp[oo++]=*p; for(const char*p=me_e;*p;p++)pp[oo++]=*p; for(const char*p="&to=";*p;p++)pp[oo++]=*p; for(const char*p=te;*p;p++)pp[oo++]=*p; for(const char*p="&body=";*p;p++)pp[oo++]=*p; for(const char*p=be;*p&&oo<490;p++)pp[oo++]=*p; pp[oo]=0;
            forum_fetch(pp);
            ui_text(5,8,"Sent.",VGA_LGREEN,VGA_BLACK); vga_statusbar(" any key"); keyboard_getc();
        }
    }
}

static void support_screen(void) {
    vga_clear(); ui_panel(2,1,76,22,"Support",VGA_LCYAN,VGA_BLACK);
    ui_text(5,3,"Send a message, idea, or contribution to Jeffery.",VGA_DGREY,VGA_BLACK);
    ui_text(5,4,"(it's reviewed before anything goes public)",VGA_DGREY,VGA_BLACK);
    ui_text(5,6,"Message:",VGA_LGREY,VGA_BLACK); char msg[200]; if(finput(14,6,msg,200)<=0) return;
    char be[400],me_e[60],pp[600]; url_enc(msg,be,sizeof be); url_enc(me,me_e,sizeof me_e);
    int o=0; for(const char*p="/support?from=";*p;p++)pp[o++]=*p; for(const char*p=me_e;*p;p++)pp[o++]=*p; for(const char*p="&body=";*p;p++)pp[o++]=*p; for(const char*p=be;*p&&o<590;p++)pp[o++]=*p; pp[o]=0;
    forum_fetch(pp);
    ui_text(5,9,"Sent to Jeffery. Thank you!",VGA_LGREEN,VGA_BLACK); vga_statusbar(" any key"); keyboard_getc();
}

static void genres_screen(void) {
    if (forum_fetch("/forum/genres") < 0) { vga_clear(); ui_panel(2,1,76,22,"Genres",VGA_LCYAN,VGA_BLACK); ui_text(5,3,"could not reach the forum server",VGA_LRED,VGA_BLACK); ui_text(5,4,"start it: python3 forum-server.py",VGA_DGREY,VGA_BLACK); vga_statusbar(" any key"); keyboard_getc(); return; }
    static char g[16][32]; int n=0, i=0;
    while (fbuf[i] && n<16) { int o=0; while(fbuf[i]&&fbuf[i]!='\n'&&o<30) g[n][o++]=fbuf[i++]; g[n][o]=0; if(fbuf[i]=='\n')i++; if(g[n][0])n++; }
    int sel=0, pbtn=0;
    for (;;) {
        vga_clear(); ui_panel(2,1,76,22,"Forums - pick a genre",VGA_LCYAN,VGA_BLACK);
        for (int k=0;k<n;k++) { int col=k%3, row=k/3; int x=6+col*23, y=4+row*4; ui_card(x,y,21,g[k],k==sel,VGA_BLUE); }
        vga_statusbar(" GENRES   arrows/mouse   Enter/click open   q back"); vga_setcursor(0,24);
        for (;;) {
            char c = keyboard_trygetc();
            if (c=='q'||c==27) return;
            else if (c==0x02) { if(sel>0)sel--; break; }
            else if (c==0x06) { if(sel<n-1)sel++; break; }
            else if (c==0x10) { if(sel>=3)sel-=3; break; }
            else if (c==0x0E) { if(sel+3<n)sel+=3; break; }
            else if (c=='\n') { posts_screen(g[sel]); break; }
            if (mouse_present()) { int mx,my,b; mouse_get(&mx,&my,&b);
                int hov=-1; for(int k=0;k<n;k++){ int col=k%3,row=k/3,x=6+col*23,y=4+row*4; if(mx>=x&&mx<x+21&&my>=y&&my<y+3) hov=k; }
                if ((b&1)&&!(pbtn&1)) { pbtn=b; if(hov>=0){ posts_screen(g[hov]); break; } }
                pbtn=b; if(hov>=0&&hov!=sel){ sel=hov; break; } }
            __asm__ volatile("hlt");
        }
    }
}

void forum_run(void) {
    if (!rtl8139_present()) { vga_clear(); ui_panel(2,1,76,22,"Hisoka Forum",VGA_LCYAN,VGA_BLACK); ui_text(5,3,"The forum needs the network (no adapter found).",VGA_LRED,VGA_BLACK); vga_statusbar(" any key"); keyboard_getc(); vga_clear(); return; }
    get_host();
    static const char *items[] = { "Browse Forums", "Search", "Inbox (DMs)", "Support / Contribute" };
    int sel = 0, pbtn = 0;
    for (;;) {
        vga_clear(); ui_panel(2,1,76,22,"Hisoka Forum",VGA_LCYAN,VGA_BLACK);
        ui_text(5,3,"Computers talking to computers.   You are:",VGA_DGREY,VGA_BLACK); ui_text(47,3,me,VGA_LCYAN,VGA_BLACK);
        for (int i=0;i<4;i++) ui_card(10, 6+i*3, 56, items[i], i==sel, VGA_BLUE);
        vga_statusbar(" FORUM   arrows/mouse   Enter/click   q quit"); vga_setcursor(0,24);
        int go = -1;
        for (;;) {
            char c = keyboard_trygetc();
            if (c=='q'||c==27) { vga_clear(); return; }
            else if (c==0x10) { sel=(sel+3)%4; break; }
            else if (c==0x0E) { sel=(sel+1)%4; break; }
            else if (c=='\n') { go=sel; break; }
            if (mouse_present()) { int mx,my,b; mouse_get(&mx,&my,&b);
                int hov=-1; for(int i=0;i<4;i++){ int y=6+i*3; if(mx>=10&&mx<66&&my>=y&&my<y+3) hov=i; }
                if ((b&1)&&!(pbtn&1)) { pbtn=b; if(hov>=0){go=hov;break;} }
                pbtn=b; if(hov>=0&&hov!=sel){sel=hov;break;} }
            __asm__ volatile("hlt");
        }
        if (go==0) genres_screen();
        else if (go==1) search_screen();
        else if (go==2) inbox_screen();
        else if (go==3) support_screen();
    }
}
