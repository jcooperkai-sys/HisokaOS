/* tools.c - real utility apps for HisokaOS (no games). Every app is fully operable
 * (features for features): create/edit/delete/persist + on-screen keybinds. */
#include "tools.h"
#include "vga.h"
#include "keyboard.h"
#include "printf.h"
#include "ramfs.h"
#include "string.h"
#include "pit.h"
#include "pmm.h"
#include "heap.h"
#include "net.h"
#include "ui.h"
#include "types.h"

/* ---- shared helpers ---- */
static uint32_t rng;
static uint32_t rnd(void) { rng = rng * 1103515245u + 12345u; return (rng >> 16) & 0x7fff; }

/* a line prompt at a given row; returns length, -1 if cancelled */
static int prompt_at(int row, const char *label, char *buf, int max, const char *preset) {
    for (int x = 1; x <= 78; x++) vga_cell(x, row, ' ', VGA_WHITE, VGA_BLACK);
    int px = 2; for (int i = 0; label[i]; i++) vga_cell(px++, row, label[i], VGA_YELLOW, VGA_BLACK);
    int len = 0; buf[0] = 0; int sx = px;
    if (preset) { for (int i = 0; preset[i] && len < max-1; i++) { buf[len] = preset[i]; vga_cell(sx+len, row, preset[i], VGA_WHITE, VGA_BLACK); len++; } buf[len] = 0; }
    vga_setcursor(sx + len, row);
    for (;;) {
        char c = keyboard_getc();
        if (c == '\n' || c == '\r') { buf[len] = 0; return len; }
        else if (c == 27) { buf[0] = 0; return -1; }
        else if (c == '\b') { if (len) { len--; vga_cell(sx+len, row, ' ', VGA_WHITE, VGA_BLACK); vga_setcursor(sx+len, row); } }
        else if (c >= 32 && c < 127 && len < max-1) { buf[len++] = c; vga_cell(sx+len-1, row, c, VGA_WHITE, VGA_BLACK); vga_setcursor(sx+len, row); }
    }
}
static long s_atol(const char *s) { long v = 0; int neg = 0; while (*s == ' ') s++; if (*s == '-') { neg = 1; s++; } while (*s >= '0' && *s <= '9') v = v*10 + (*s++ - '0'); return neg ? -v : v; }
static char lc(char c) { return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c; }

/* ======================= TODO ======================= */
#define TMAX 64
#define TLEN 76
static char  t_text[TMAX][TLEN];
static int   t_done[TMAX];
static int   t_n;

static void todo_load(void) {
    t_n = 0;
    fs_file_t *f = fs_find("/home/todo.txt");
    if (!f || f->is_dir) return;
    int i = 0; char line[TLEN]; int li = 0;
    for (size_t k = 0; k <= f->len && t_n < TMAX; k++) {
        char c = (k < f->len) ? (char)f->data[k] : '\n';
        if (c == '\n') {
            line[li] = 0;
            if (li > 0) {
                int done = 0, off = 0;
                if (line[0] == '[' && (line[1] == 'x' || line[1] == 'X') && line[2] == ']') { done = 1; off = (line[3] == ' ') ? 4 : 3; }
                else if (line[0] == '[' && line[2] == ']') { done = 0; off = (line[3] == ' ') ? 4 : 3; }
                strncpy(t_text[t_n], line + off, TLEN-1); t_text[t_n][TLEN-1] = 0; t_done[t_n] = done; t_n++;
            }
            li = 0;
        } else if (li < TLEN-1) line[li++] = c;
        (void)i;
    }
}
static void todo_save(void) {
    static char out[TMAX * (TLEN + 5)]; int o = 0;
    for (int i = 0; i < t_n; i++) {
        out[o++] = '['; out[o++] = t_done[i] ? 'x' : ' '; out[o++] = ']'; out[o++] = ' ';
        for (int k = 0; t_text[i][k]; k++) out[o++] = t_text[i][k];
        out[o++] = '\n';
    }
    fs_write("/home/todo.txt", out, (size_t)o);
}
void todo_run(void) {
    todo_load();
    int sel = 0;
    for (;;) {
        vga_clear();
        char title[24]; int tn = 0; for (const char *p = "Todo  ("; *p; p++) title[tn++] = *p;
        { int v = t_n, m = 0; char tmp[6]; if (!v) tmp[m++]='0'; while (v){tmp[m++]=(char)('0'+v%10);v/=10;} while (m) title[tn++]=tmp[--m]; }
        title[tn++] = ')'; title[tn] = 0;
        ui_panel(2, 1, 76, 22, title, VGA_LCYAN, VGA_BLACK);
        if (!t_n) ui_text(6, 4, "No tasks yet. Press 'a' to add one.", VGA_DGREY, VGA_BLACK);
        for (int i = 0; i < t_n; i++) {
            int y = 3 + i; if (y > 20) break;
            uint8_t fg = (i==sel) ? VGA_BLACK : (t_done[i] ? VGA_DGREY : VGA_LGREY);
            uint8_t bg = (i==sel) ? VGA_LGREY : VGA_BLACK;
            for (int x = 4; x <= 75; x++) vga_cell(x, y, ' ', fg, bg);
            vga_cell(4, y, '[', fg, bg); vga_cell(5, y, t_done[i] ? 'x' : ' ', t_done[i] ? VGA_LGREEN : fg, bg); vga_cell(6, y, ']', fg, bg);
            int x = 8; for (int k = 0; t_text[i][k] && x <= 75; k++) vga_cell(x++, y, t_text[i][k], fg, bg);
        }
        vga_statusbar(" TODO  Up/Dn  Space done  a add  e edit  d del  [ ] reorder  c clear  q quit");
        vga_setcursor(0, 24);
        char k = keyboard_getc();
        if (k == 'q' || k == 27) { todo_save(); break; }
        else if (k == 0x10) { if (sel > 0) sel--; }
        else if (k == 0x0E) { if (sel < t_n-1) sel++; }
        else if (k == ' ') { if (t_n) { t_done[sel] = !t_done[sel]; todo_save(); } }
        else if (k == 'a') { if (t_n < TMAX) { char b[TLEN]; if (prompt_at(23, "New task: ", b, TLEN, 0) > 0) { strcpy(t_text[t_n], b); t_done[t_n] = 0; sel = t_n; t_n++; todo_save(); } } }
        else if (k == 'e') { if (t_n) { char b[TLEN]; if (prompt_at(23, "Edit: ", b, TLEN, t_text[sel]) >= 0) { strcpy(t_text[sel], b); todo_save(); } } }
        else if (k == 'd') { if (t_n) { for (int i = sel; i < t_n-1; i++) { strcpy(t_text[i], t_text[i+1]); t_done[i] = t_done[i+1]; } t_n--; if (sel >= t_n && sel > 0) sel--; todo_save(); } }
        else if (k == '[') { if (sel > 0) { char tmp[TLEN]; strcpy(tmp, t_text[sel]); strcpy(t_text[sel], t_text[sel-1]); strcpy(t_text[sel-1], tmp); int d = t_done[sel]; t_done[sel] = t_done[sel-1]; t_done[sel-1] = d; sel--; todo_save(); } }
        else if (k == ']') { if (sel < t_n-1) { char tmp[TLEN]; strcpy(tmp, t_text[sel]); strcpy(t_text[sel], t_text[sel+1]); strcpy(t_text[sel+1], tmp); int d = t_done[sel]; t_done[sel] = t_done[sel+1]; t_done[sel+1] = d; sel++; todo_save(); } }
        else if (k == 'c') { int w = 0; for (int i = 0; i < t_n; i++) if (!t_done[i]) { strcpy(t_text[w], t_text[i]); t_done[w] = 0; w++; } t_n = w; if (sel >= t_n) sel = t_n ? t_n-1 : 0; todo_save(); }
    }
    vga_statusbar("help  commands  man       HisokaOS 0.2  i386"); vga_clear();
    vga_setcolor(VGA_LCYAN, VGA_BLACK); kputs("Todo closed (saved to /home/todo.txt).\n"); vga_setcolor(VGA_LGREY, VGA_BLACK);
}

/* ======================= SYSTEM MONITOR ======================= */
/* draw "label<number><unit>" at (x,y) inside a box */
static void ui_kv(int x, int y, const char *k, uint32_t v, const char *unit, uint8_t fg) {
    char b[48]; int n = 0;
    for (const char *p = k; *p && n < 40; p++) b[n++] = *p;
    char t[12]; int m = 0; uint32_t vv = v; if (!vv) t[m++]='0'; while (vv) { t[m++]=(char)('0'+vv%10); vv/=10; } while (m) b[n++]=t[--m];
    for (const char *p = unit; *p && n < 46; p++) b[n++] = *p; b[n] = 0;
    ui_text(x, y, b, fg, VGA_BLACK);
}
void monitor_run(void) {
    int paused = 0;
    for (;;) {
        if (!paused) {
            vga_clear();
            ui_panel(2, 1, 76, 22, "System Monitor", VGA_LCYAN, VGA_BLACK);
            uint32_t tot = pmm_total_frames(), fr = pmm_free_frames();
            uint32_t up = tot ? (tot - fr) * 100 / tot : 0;
            ui_kv(5, 3, "Memory used  : ", (tot-fr)*4/1024, " MiB", VGA_LGREY);
            ui_kv(5, 4, "Memory total : ", tot*4/1024, " MiB", VGA_LGREY);
            ui_kv(5, 5, "Usage        : ", up, " %", VGA_LGREY);
            int bw = 60, fill = (int)(up * bw / 100);
            ui_box(5, 6, bw + 2, 3, VGA_DGREY, VGA_BLACK);
            for (int x = 0; x < bw; x++) vga_cell(6 + x, 7, ' ', VGA_WHITE, x < fill ? VGA_LGREEN : VGA_DGREY);
            ui_kv(5, 10, "Heap in use  : ", (uint32_t)heap_used(), " bytes", VGA_LGREY);
            ui_kv(5, 11, "Files        : ", (uint32_t)fs_count(), " in filesystem", VGA_LGREY);
            ui_kv(5, 12, "Uptime       : ", pit_ticks() / 100, " s", VGA_LGREY);
            const uint8_t *ip = net_my_ip();
            char nb[40]; int n = 0; for (const char *p = "Network      : "; *p; p++) nb[n++] = *p;
            for (int o = 0; o < 4; o++) { uint32_t vv = ip[o], m = 0; char t[4]; if (!vv) t[m++]='0'; while (vv){t[m++]=(char)('0'+vv%10);vv/=10;} while (m) nb[n++]=t[--m]; if (o<3) nb[n++]='.'; }
            nb[n] = 0; ui_text(5, 13, nb, VGA_LGREY, VGA_BLACK);
        }
        vga_statusbar(paused ? " MONITOR  [PAUSED]   p resume   q quit" : " SYSTEM MONITOR   live   p pause   q quit");
        vga_setcursor(0, 24);
        uint32_t s = pit_ticks();
        while (pit_ticks() - s < 40) {
            char c = keyboard_trygetc();
            if (c == 'q' || c == 27) { vga_clear(); vga_setcolor(VGA_LCYAN, VGA_BLACK); kputs("Monitor closed.\n"); vga_setcolor(VGA_LGREY, VGA_BLACK); return; }
            if (c == 'p') { paused = !paused; break; }
            __asm__ volatile("hlt");
        }
    }
}

/* ======================= UNITS CONVERTER ======================= */
/* length to millimetres (approx, integer) */
static long len_mm(const char *u) {
    if (!strcmp(u,"mm")) return 1; if (!strcmp(u,"cm")) return 10; if (!strcmp(u,"m")) return 1000;
    if (!strcmp(u,"km")) return 1000000; if (!strcmp(u,"in")) return 25; if (!strcmp(u,"ft")) return 305;
    if (!strcmp(u,"yd")) return 914; if (!strcmp(u,"mi")) return 1609344; return 0;
}
static long wt_mg(const char *u) {
    if (!strcmp(u,"mg")) return 1; if (!strcmp(u,"g")) return 1000; if (!strcmp(u,"kg")) return 1000000;
    if (!strcmp(u,"oz")) return 28350; if (!strcmp(u,"lb")) return 453592; return 0;
}
/* read a line at a fixed (x,y) echoing in white; returns len, -1 on Esc */
static int box_input(int x, int y, char *buf, int max) {
    int len = 0; buf[0] = 0; vga_setcursor(x, y);
    for (;;) {
        char c = keyboard_getc();
        if (c=='\n'||c=='\r') { buf[len]=0; return len; }
        else if (c==27) { buf[0]=0; return -1; }
        else if (c=='\b') { if (len) { len--; vga_cell(x+len,y,' ',VGA_WHITE,VGA_BLACK); vga_setcursor(x+len,y); } }
        else if (c>=32&&c<127&&len<max-1) { buf[len++]=c; vga_cell(x+len-1,y,c,VGA_WHITE,VGA_BLACK); vga_setcursor(x+len,y); }
    }
}
void units_run(void) {
    char result[48] = {0}; uint8_t rc = VGA_LCYAN;
    for (;;) {
        vga_clear();
        ui_panel(2, 1, 76, 22, "Units Converter", VGA_LCYAN, VGA_BLACK);
        ui_text(5, 3, "Type:  <value> <from> <to>      e.g.  100 c f", VGA_DGREY, VGA_BLACK);
        ui_text(5, 5, "Temp   : c f k", VGA_LGREY, VGA_BLACK);
        ui_text(5, 6, "Length : mm cm m km in ft yd mi", VGA_LGREY, VGA_BLACK);
        ui_text(5, 7, "Weight : mg g kg oz lb", VGA_LGREY, VGA_BLACK);
        ui_text(5, 9, ">", VGA_LMAGENTA, VGA_BLACK);
        if (result[0]) ui_text(5, 12, result, rc, VGA_BLACK);
        vga_statusbar(" UNITS   type a conversion + Enter   Esc to quit");
        char line[64];
        int r = box_input(7, 9, line, 64);
        if (r < 0) { vga_clear(); return; }
        if (r == 1 && (line[0]=='q'||line[0]=='Q')) { vga_clear(); return; }
        if (r == 0) continue;
        char val[24], fu[8], tu[8]; int p = 0, q = 0;
        while (line[p]==' ') p++; while (line[p] && line[p]!=' ' && q<23) val[q++]=line[p++]; val[q]=0;
        while (line[p]==' ') p++; q=0; while (line[p] && line[p]!=' ' && q<7) fu[q++]=lc(line[p++]); fu[q]=0;
        while (line[p]==' ') p++; q=0; while (line[p] && line[p]!=' ' && q<7) tu[q++]=lc(line[p++]); tu[q]=0;
        long v = s_atol(val), out = 0; int ok = 1;
        if ((!strcmp(fu,"c")||!strcmp(fu,"f")||!strcmp(fu,"k")) && (!strcmp(tu,"c")||!strcmp(tu,"f")||!strcmp(tu,"k"))) {
            long cdeg = !strcmp(fu,"c") ? v : !strcmp(fu,"f") ? (v-32)*5/9 : v-273;
            out = !strcmp(tu,"c") ? cdeg : !strcmp(tu,"f") ? cdeg*9/5+32 : cdeg+273;
        } else if (len_mm(fu) && len_mm(tu)) out = v * len_mm(fu) / len_mm(tu);
        else if (wt_mg(fu) && wt_mg(tu)) out = v * wt_mg(fu) / wt_mg(tu);
        else ok = 0;
        if (ok) {
            int n = 0; result[n++]='='; result[n++]=' ';
            long ov = out<0?-out:out; char t[12]; int tn=0; if(!ov)t[tn++]='0'; while(ov){t[tn++]=(char)('0'+ov%10);ov/=10;} if(out<0)result[n++]='-'; while(tn)result[n++]=t[--tn];
            result[n++]=' '; for (int k=0; tu[k]; k++) result[n++]=tu[k]; result[n]=0; rc=VGA_LCYAN;
        } else { strcpy(result, "can't convert those units (mixed categories?)"); rc=VGA_LRED; }
    }
}

/* ======================= STOPWATCH ======================= */
/* format centiseconds as "M:SS.CC" into o; returns length */
static int fmt_cs(uint32_t e, char *o) {
    uint32_t mn = e/6000, sc = (e/100)%60, cs = e%100; int n = 0;
    { uint32_t v = mn; char t[6]; int m=0; if(!v)t[m++]='0'; while(v){t[m++]=(char)('0'+v%10);v/=10;} while(m)o[n++]=t[--m]; }
    o[n++]=':'; o[n++]=(char)('0'+sc/10); o[n++]=(char)('0'+sc%10);
    o[n++]='.'; o[n++]=(char)('0'+cs/10); o[n++]=(char)('0'+cs%10); o[n]=0; return n;
}
void stopwatch_run(void) {
    uint32_t start = pit_ticks(); int running = 1;
    uint32_t laps[12]; int nlap = 0;
    for (;;) {
        uint32_t e = (running ? pit_ticks() - start : start);
        vga_clear();
        ui_panel(2, 1, 76, 22, "Stopwatch", VGA_LCYAN, VGA_BLACK);
        ui_box(22, 3, 32, 3, VGA_DGREY, VGA_BLACK);
        char tb[24]; fmt_cs(e, tb); ui_text(26, 4, tb, VGA_WHITE, VGA_BLACK);
        ui_text(40, 4, running ? "running" : "stopped", running ? VGA_LGREEN : VGA_DGREY, VGA_BLACK);
        for (int i = 0; i < nlap && i < 12; i++) {
            char lb[28]; int n = 0; for (const char *p = "lap "; *p; p++) lb[n++] = *p;
            lb[n++] = (char)('0' + (i+1)/10); if ((i+1) < 10) n--; lb[n++] = (char)('0' + (i+1)%10);
            lb[n++] = ':'; lb[n++] = ' '; fmt_cs(laps[i], lb + n);
            ui_text(8, 8 + i, lb, VGA_LGREY, VGA_BLACK);
        }
        vga_statusbar(" STOPWATCH   space start/stop   l lap   r reset   q quit");
        vga_setcursor(0, 24);
        uint32_t s = pit_ticks();
        int act = 0;
        while (pit_ticks() - s < 8) {
            char c = keyboard_trygetc();
            if (c == 'q' || c == 27) { vga_clear(); vga_setcolor(VGA_LCYAN, VGA_BLACK); kputs("Stopwatch closed.\n"); vga_setcolor(VGA_LGREY, VGA_BLACK); return; }
            if (c == ' ') { if (running) { start = pit_ticks() - start; running = 0; } else { start = pit_ticks() - start; running = 1; } act = 1; break; }
            if (c == 'l') { if (nlap < 12) laps[nlap++] = e; act = 1; break; }
            if (c == 'r') { start = pit_ticks(); nlap = 0; running = 1; act = 1; break; }
            __asm__ volatile("hlt");
        }
        (void)act;
    }
}

/* ======================= BASE CONVERTER ======================= */
static int base_str(uint32_t v, int base, char *o) {
    char d[40]; const char *D = "0123456789abcdef"; int n = 0;
    if (v == 0) d[n++] = '0'; else while (v) { d[n++] = D[v % base]; v /= base; }
    int m = 0; while (n--) o[m++] = d[n]; o[m] = 0; return m;
}
void baseconv_run(void) {
    char dec[40], hx[40], oc[40], bn[40]; int have = 0;
    for (;;) {
        vga_clear();
        ui_panel(2, 1, 76, 22, "Base Converter", VGA_LCYAN, VGA_BLACK);
        ui_text(5, 3, "Enter a number:  42   0x2a   0b101010   0o52", VGA_DGREY, VGA_BLACK);
        ui_text(5, 5, ">", VGA_LMAGENTA, VGA_BLACK);
        if (have) {
            ui_text(5, 8,  "dec", VGA_DGREY, VGA_BLACK); ui_text(12, 8,  dec, VGA_LCYAN, VGA_BLACK);
            ui_text(5, 9,  "hex", VGA_DGREY, VGA_BLACK); ui_text(12, 9,  "0x", VGA_DGREY, VGA_BLACK); ui_text(14, 9,  hx, VGA_LCYAN, VGA_BLACK);
            ui_text(5, 10, "oct", VGA_DGREY, VGA_BLACK); ui_text(12, 10, "0o", VGA_DGREY, VGA_BLACK); ui_text(14, 10, oc, VGA_LCYAN, VGA_BLACK);
            ui_text(5, 11, "bin", VGA_DGREY, VGA_BLACK); ui_text(12, 11, "0b", VGA_DGREY, VGA_BLACK); ui_text(14, 11, bn, VGA_LCYAN, VGA_BLACK);
        }
        vga_statusbar(" BASE   enter a number + Enter   Esc to quit");
        char line[40];
        int r = box_input(7, 5, line, 40);
        if (r < 0) { vga_clear(); return; }
        if (r == 1 && (line[0]=='q'||line[0]=='Q')) { vga_clear(); return; }
        if (r == 0) continue;
        uint32_t v = 0; const char *s = line; while (*s == ' ') s++;
        if (s[0]=='0' && (s[1]=='x'||s[1]=='X')) { s += 2; for (; *s; s++) { char c = lc(*s); if (c>='0'&&c<='9') v=v*16+(c-'0'); else if (c>='a'&&c<='f') v=v*16+(c-'a'+10); } }
        else if (s[0]=='0' && (s[1]=='b'||s[1]=='B')) { s += 2; for (; *s; s++) if (*s=='0'||*s=='1') v=v*2+(*s-'0'); }
        else if (s[0]=='0' && (s[1]=='o'||s[1]=='O')) { s += 2; for (; *s; s++) if (*s>='0'&&*s<='7') v=v*8+(*s-'0'); }
        else { for (; *s; s++) if (*s>='0'&&*s<='9') v=v*10+(*s-'0'); }
        base_str(v,10,dec); base_str(v,16,hx); base_str(v,8,oc); base_str(v,2,bn); have = 1;
    }
}

/* ======================= PASSWORD GENERATOR ======================= */
void passgen_run(void) {
    int length = 12, useL = 1, useD = 1, useS = 1;
    char cur[64];
    rng = pit_ticks() + 7;
    #define GEN() do { \
        char set[96]; int sn = 0; \
        if (useL) for (char c='a'; c<='z'; c++) set[sn++]=c; \
        if (useL) for (char c='A'; c<='Z'; c++) set[sn++]=c; \
        if (useD) for (char c='0'; c<='9'; c++) set[sn++]=c; \
        if (useS) { const char *sym="!@#$%^&*-_=+?"; for (int i=0; sym[i]; i++) set[sn++]=sym[i]; } \
        if (sn==0) { set[sn++]='a'; } \
        for (int i=0;i<length;i++) cur[i]=set[rnd()%sn]; cur[length]=0; \
    } while(0)
    GEN();
    for (;;) {
        vga_clear();
        ui_panel(2, 1, 76, 22, "Password Generator", VGA_LCYAN, VGA_BLACK);
        ui_box(6, 3, 66, 3, VGA_DGREY, VGA_BLACK);
        ui_text(8, 4, cur, VGA_LCYAN, VGA_BLACK);
        ui_kv(6, 7, "length  : ", (uint32_t)length, "    (+/- to change)", VGA_LGREY);
        ui_text(6, 8,  useL ? "letters : on    (l)" : "letters : off   (l)", VGA_LGREY, VGA_BLACK);
        ui_text(6, 9,  useD ? "digits  : on    (d)" : "digits  : off   (d)", VGA_LGREY, VGA_BLACK);
        ui_text(6, 10, useS ? "symbols : on    (y)" : "symbols : off   (y)", VGA_LGREY, VGA_BLACK);
        vga_statusbar(" PASSGEN   g/space regenerate   +/- length   l/d/y toggle   s save   q quit");
        vga_setcursor(0, 24);
        char k = keyboard_getc();
        if (k == 'q' || k == 27) break;
        else if (k == 'g' || k == ' ') { GEN(); }
        else if (k == '+' || k == '=') { if (length < 48) { length++; GEN(); } }
        else if (k == '-' || k == '_') { if (length > 4) { length--; GEN(); } }
        else if (k == 'l') { useL = !useL; GEN(); }
        else if (k == 'd') { useD = !useD; GEN(); }
        else if (k == 'y') { useS = !useS; GEN(); }
        else if (k == 's') { fs_mkdir("/home/secrets"); char buf[80]; int o=0; for (int i=0;cur[i];i++) buf[o++]=cur[i]; buf[o++]='\n'; fs_append("/home/secrets/passwords.txt", buf, (size_t)o); }
    }
    #undef GEN
    vga_statusbar("help  commands  man       HisokaOS 0.2  i386"); vga_clear();
    vga_setcolor(VGA_LCYAN, VGA_BLACK); kputs("Password Generator closed.\n"); vga_setcolor(VGA_LGREY, VGA_BLACK);
}
