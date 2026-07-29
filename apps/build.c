/* build.c - Build: the HisokaOS code editor, VS Code / Zed style.
 *
 * Two panes: a FILE SIDEBAR on the left (browse the real filesystem, open or create
 * files and folders) and the editor on the right (line numbers, syntax highlighting,
 * find, goto). It opens on the sidebar - a menu - instead of dropping you into a blank
 * file. Simple keys: in the sidebar everything is a single key (Enter, N, F, D); in the
 * editor, Esc goes back to the files and Ctrl+S saves. */
#include "build.h"
#include "vga.h"
#include "keyboard.h"
#include "ramfs.h"
#include "printf.h"
#include "string.h"
#include "types.h"

/* ---- layout ---- */
#define SBW     18                 /* sidebar columns 1..18              */
#define SEPC    19                 /* separator column                   */
#define EDX0    20                 /* editor area first column           */
#define GUT     5                  /* gutter: 4 digits + a space         */
#define TX0     25                 /* editor text first column           */
#define TXW     54                 /* editor text width (25..78)         */
#define EROWS   23                 /* editor rows 1..23                  */
#define SBTOP   2                  /* sidebar list first row             */
#define SBROWS  22                 /* sidebar list rows 2..23            */
#define MAXBUF  16384

/* ---- keys ---- */
#define K_LEFT 0x02
#define K_RIGHT 0x06
#define K_UP   0x10
#define K_DOWN 0x0E
#define K_HOME 0x01
#define K_END  0x05
#define K_PGUP 0x19
#define K_PGDN 0x1A
#define K_DEL  0x7F
#define K_BKSP 0x08
#define K_TAB  0x09
#define K_SAVE 0x13                /* Ctrl-S */
#define K_QUIT 0x11                /* Ctrl-Q */
#define K_GOTO 0x07                /* Ctrl-G */
#define K_FIND 0x17                /* Ctrl-W */
#define K_ESC  27

enum { F_SIDEBAR, F_EDITOR };

/* ---- editor document ---- */
static char buf[MAXBUF];
static int  len, cur, modified, hasfile;
static char fname[FS_NAME_LEN];

/* ---- sidebar state ---- */
static char sbdir[FS_NAME_LEN];
static char li_name[64][FS_NAME_LEN];
static char li_path[64][FS_NAME_LEN];
static int  li_dir[64], li_n;

/* ---- syntax colors ---- */
#define C_TEXT VGA_LGREY
#define C_COMMENT VGA_DGREY
#define C_STRING VGA_LGREEN
#define C_NUMBER VGA_LCYAN
#define C_KEYWORD VGA_YELLOW
enum { LANG_TEXT, LANG_C, LANG_SH };

static int lang_of(const char *name) {
    const char *dot = 0; for (const char *p = name; *p; p++) if (*p == '.') dot = p + 1;
    if (!dot) return LANG_SH;
    if (!strcmp(dot,"c")||!strcmp(dot,"h")||!strcmp(dot,"json")||!strcmp(dot,"js")||!strcmp(dot,"cpp")) return LANG_C;
    if (!strcmp(dot,"sh")||!strcmp(dot,"py")||!strcmp(dot,"cfg")||!strcmp(dot,"conf")||
        !strcmp(dot,"ini")||!strcmp(dot,"yaml")||!strcmp(dot,"yml")||!strcmp(dot,"rb")) return LANG_SH;
    return LANG_TEXT;
}
static const char *lang_name(int l) { return l == LANG_C ? "c" : l == LANG_SH ? "sh" : "text"; }
static const char *KW[] = {
    "if","else","for","while","do","return","break","continue","switch","case","default",
    "int","char","void","long","short","unsigned","signed","float","double","struct","union",
    "enum","typedef","static","const","extern","sizeof","goto","include","define","ifndef",
    "ifdef","endif","echo","then","fi","done","local","export","function","def","class",
    "import","from","print","true","false","null","new","let","var", 0
};
static int is_alpha(char c) { return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||c=='_'; }
static int is_alnum(char c) { return is_alpha(c)||(c>='0'&&c<='9'); }
static int is_kw(const char *w) { for (int i = 0; KW[i]; i++) if (!strcmp(w, KW[i])) return 1; return 0; }
static void color_line(const char *s, int n, uint8_t *col, int lang) {
    for (int i = 0; i < n; i++) col[i] = C_TEXT;
    if (lang == LANG_TEXT) return;
    int i = 0;
    while (i < n) {
        char c = s[i];
        if ((lang==LANG_SH && c=='#') || (lang==LANG_C && c=='/' && i+1<n && s[i+1]=='/')) { for (; i<n; i++) col[i]=C_COMMENT; break; }
        if (c=='"'||c=='\'') { char q=c; col[i]=C_STRING; i++; while (i<n){ col[i]=C_STRING; if (s[i]==q){i++;break;} i++; } continue; }
        if (c>='0'&&c<='9') { while (i<n && ((s[i]>='0'&&s[i]<='9')||s[i]=='.'||s[i]=='x'||(s[i]>='a'&&s[i]<='f')||(s[i]>='A'&&s[i]<='F'))) { col[i]=C_NUMBER; i++; } continue; }
        if (is_alpha(c)) { int s0=i; char w[40]; int wl=0; while (i<n && is_alnum(s[i])) { if (wl<39) w[wl++]=s[i]; i++; } w[wl]=0; if (is_kw(w)) for (int k=s0;k<i;k++) col[k]=C_KEYWORD; continue; }
        i++;
    }
}

/* ---- path helpers ---- */
static void bp_parent(const char *p, char *out) {
    int last = 0, n = (int)strlen(p);
    for (int i = 0; i < n; i++) if (p[i] == '/') last = i;
    if (last == 0) strcpy(out, "/"); else { memcpy(out, p, last); out[last] = 0; }
}
static const char *bp_base(const char *p) { const char *b = p; for (const char *q = p; *q; q++) if (*q == '/') b = q + 1; return b; }
static void bp_join(const char *dir, const char *name, char *out) {
    int o = 0; for (const char *p = dir; *p; p++) out[o++] = *p;
    if (!(o == 1 && out[0] == '/')) out[o++] = '/';
    for (const char *p = name; *p && o < FS_NAME_LEN - 1; p++) out[o++] = *p;
    out[o] = 0;
}

/* ---- line/index math ---- */
static int total_lines(void) { int n = 1; for (int i = 0; i < len; i++) if (buf[i] == '\n') n++; return n; }
static int line_start(int line) { if (line <= 0) return 0; int n = 0; for (int i = 0; i < len; i++) if (buf[i] == '\n') { if (++n == line) return i+1; } return len; }
static int line_end(int line) { int i = line_start(line); while (i < len && buf[i] != '\n') i++; return i; }
static void cursor_lc(int *pl, int *pc) { int l=0,c=0; for (int i=0;i<cur;i++){ if (buf[i]=='\n'){l++;c=0;} else c++; } *pl=l; *pc=c; }
static int index_lc(int line, int col) { if (line<0) line=0; int s=line_start(line),e=line_end(line),ln=e-s; if (col>ln) col=ln; return s+col; }
static void insert(char c) { if (len>=MAXBUF) return; for (int i=len;i>cur;i--) buf[i]=buf[i-1]; buf[cur++]=c; len++; modified=1; }
static void backspace(void) { if (cur==0) return; for (int i=cur-1;i<len-1;i++) buf[i]=buf[i+1]; len--; cur--; modified=1; }
static void del_forward(void) { if (cur>=len) return; for (int i=cur;i<len-1;i++) buf[i]=buf[i+1]; len--; modified=1; }

static int find_from(const char *q, int from) {
    int ql=(int)strlen(q); if (!ql) return -1;
    for (int i=from;i+ql<=len;i++){ int k=0; while (k<ql && buf[i+k]==q[k]) k++; if (k==ql) return i; }
    return -1;
}

/* a prompt on the bottom row */
static int prompt_line(const char *label, char *out, int max) {
    for (int x=1;x<=78;x++) vga_cell(x,23,' ',VGA_WHITE,VGA_BLACK);
    int px=2; for (int i=0;label[i];i++) vga_cell(px++,23,label[i],VGA_YELLOW,VGA_BLACK);
    int l=0; out[0]=0; int sx=px; vga_setcursor(sx,23);
    for (;;) {
        char c = keyboard_getc();
        if (c=='\n'||c=='\r') { out[l]=0; return l>0; }
        else if (c==27) { out[0]=0; return 0; }
        else if (c==K_BKSP) { if (l) { l--; vga_cell(sx+l,23,' ',VGA_WHITE,VGA_BLACK); vga_setcursor(sx+l,23); } }
        else if (c>=32&&c<127&&l<max-1) { out[l++]=c; vga_cell(sx+l-1,23,c,VGA_WHITE,VGA_BLACK); vga_setcursor(sx+l,23); }
    }
}

/* ---- sidebar ---- */
static void build_list(void) {
    li_n = 0;
    if (strcmp(sbdir, "/")) { strcpy(li_name[li_n], ".."); bp_parent(sbdir, li_path[li_n]); li_dir[li_n] = 1; li_n++; }
    for (int pass = 0; pass < 2; pass++)
        for (int i = 0; i < FS_MAX_FILES && li_n < 64; i++) {
            fs_file_t *f = fs_at(i); if (!f || !f->used) continue;
            char pd[FS_NAME_LEN]; bp_parent(f->name, pd);
            if (strcmp(pd, sbdir)) continue;
            if (pass == 0 && !f->is_dir) continue;
            if (pass == 1 && f->is_dir) continue;
            strcpy(li_name[li_n], bp_base(f->name)); strcpy(li_path[li_n], f->name); li_dir[li_n] = f->is_dir; li_n++;
        }
}
static void load_file(const char *path) {
    int n = 0; while (path[n] && n < FS_NAME_LEN-1) { fname[n] = path[n]; n++; } fname[n] = 0;
    fs_file_t *f = fs_find(fname);
    len = 0;
    if (f && !f->is_dir) for (size_t i = 0; i < f->len && len < MAXBUF; i++) buf[len++] = (char)f->data[i];
    cur = 0; modified = 0; hasfile = 1;
}
static void draw_sep(void) { for (int y = 1; y <= 23; y++) vga_cell(SEPC, y, '|', VGA_DGREY, VGA_BLACK); }
static void draw_sidebar(int active, int sel, int sbtop) {
    for (int x = 1; x <= SBW; x++) vga_cell(x, 1, ' ', VGA_WHITE, active ? VGA_BLUE : VGA_DGREY);
    const char *hdr = "FILES";
    for (int i = 0; hdr[i]; i++) vga_cell(2 + i, 1, hdr[i], VGA_WHITE, active ? VGA_BLUE : VGA_DGREY);
    for (int r = 0; r < SBROWS; r++) {
        int idx = sbtop + r, y = SBTOP + r; if (idx >= li_n) break;
        uint8_t fg = (active && idx==sel) ? VGA_BLACK : (li_dir[idx] ? VGA_LCYAN : VGA_LGREY);
        uint8_t bg = (active && idx==sel) ? VGA_LGREY : VGA_BLACK;
        for (int x = 1; x <= SBW; x++) vga_cell(x, y, ' ', fg, bg);
        int x = 2; for (int k = 0; li_name[idx][k] && x <= SBW-1; k++) vga_cell(x++, y, li_name[idx][k], fg, bg);
        if (li_dir[idx] && x <= SBW) vga_cell(x, y, '/', fg, bg);
    }
}
static void draw_gutter(int row, int lineno, int iscur) {
    char d[4]; int v = lineno;
    for (int k = 3; k >= 0; k--) { d[k] = v > 0 ? (char)('0'+v%10) : ' '; v /= 10; }
    uint8_t fg = iscur ? VGA_YELLOW : VGA_DGREY;
    for (int k = 0; k < 4; k++) vga_cell(EDX0 + k, row, d[k], fg, VGA_BLACK);
    vga_cell(EDX0 + 4, row, ' ', fg, VGA_BLACK);
}
static void draw_editor(int active, int top, int coloff, int lang) {
    if (!hasfile) {
        const char *l1 = "Build - code editor";
        const char *l2 = "Pick a file on the left and press Enter,";
        const char *l3 = "or press N to create a new file.";
        const char *l4 = "Esc quits.  Ctrl+S saves while editing.";
        for (int i = 0; l1[i]; i++) vga_cell(TX0 + i, 3, l1[i], VGA_LCYAN, VGA_BLACK);
        for (int i = 0; l2[i]; i++) vga_cell(TX0 + i, 5, l2[i], VGA_LGREY, VGA_BLACK);
        for (int i = 0; l3[i]; i++) vga_cell(TX0 + i, 6, l3[i], VGA_LGREY, VGA_BLACK);
        for (int i = 0; l4[i]; i++) vga_cell(TX0 + i, 8, l4[i], VGA_DGREY, VGA_BLACK);
        return;
    }
    int curl, curc; cursor_lc(&curl, &curc);
    int tl = total_lines();
    static char tmp[600]; static uint8_t col[600];
    for (int r = 0; r < EROWS; r++) {
        int line = top + r; if (line >= tl) break;
        draw_gutter(1 + r, line + 1, active && line == curl);
        int s = line_start(line), e = line_end(line), n = e - s; if (n > 599) n = 599;
        for (int i = 0; i < n; i++) tmp[i] = buf[s + i];
        color_line(tmp, n, col, lang);
        for (int x = 0; x < TXW; x++) { int ci = coloff + x; if (ci < n) vga_cell(TX0 + x, 1 + r, tmp[ci], col[ci], VGA_BLACK); }
    }
}

static void set_status(int focus, int lang) {
    char sb[80]; int k = 0;
    #define ADD(str) do { for (const char *p=(str); *p && k<79; p++) sb[k++]=*p; } while (0)
    #define ADDN(v)  do { char t[8]; int m=0,x=(v); if(!x)t[m++]='0'; while(x){t[m++]=(char)('0'+x%10);x/=10;} while(m&&k<79)sb[k++]=t[--m]; } while (0)
    if (focus == F_SIDEBAR) {
        ADD(" FILES   Up/Down move   Enter open   N new   F folder   D delete   Esc quit");
    } else {
        int cl, cc; cursor_lc(&cl, &cc);
        ADD(" EDIT  "); ADD(lang_name(lang)); ADD("  Ln "); ADDN(cl+1); ADD(" Col "); ADDN(cc+1);
        ADD("   Ctrl+S save  Esc files  Ctrl+W find  Ctrl+G go");
    }
    #undef ADD
    #undef ADDN
    sb[k] = 0; vga_statusbar(sb);
}

/* returns 1 to actually quit (handling unsaved changes) */
static int confirm_quit(void) {
    if (!modified) return 1;
    for (int x = 1; x <= 78; x++) vga_cell(x, 23, ' ', VGA_WHITE, VGA_BLACK);
    const char *m = "Save changes?  y = save   n = discard   Esc = cancel";
    int x = 2; for (int i = 0; m[i]; i++) vga_cell(x++, 23, m[i], VGA_YELLOW, VGA_BLACK);
    char a = keyboard_getc();
    if (a == 'y' || a == 'Y') { fs_write(fname, buf, (size_t)len); modified = 0; return 1; }
    if (a == 'n' || a == 'N') return 1;
    return 0;
}

void build_run(const char *path) {
    /* decide initial state from the path */
    len = 0; cur = 0; modified = 0; hasfile = 0;
    strcpy(sbdir, "/home");
    int focus = F_SIDEBAR, lang = LANG_TEXT;
    if (path && *path) {
        fs_file_t *f = fs_find(path);
        if (f && !f->is_dir) { load_file(path); bp_parent(path, sbdir); focus = F_EDITOR; lang = lang_of(fname); }
        else if (f && f->is_dir) { strcpy(sbdir, path); }
    }

    int sel = 0, sbtop = 0, top = 0, coloff = 0, quit = 0;
    while (!quit) {
        build_list();
        if (sel >= li_n) sel = li_n ? li_n - 1 : 0; if (sel < 0) sel = 0;
        if (sel < sbtop) sbtop = sel;
        if (sel >= sbtop + SBROWS) sbtop = sel - SBROWS + 1;

        if (focus == F_EDITOR && hasfile) {
            int cl, cc; cursor_lc(&cl, &cc);
            if (cl < top) top = cl;
            if (cl >= top + EROWS) top = cl - EROWS + 1;
            if (cc < coloff) coloff = cc;
            if (cc >= coloff + TXW) coloff = cc - TXW + 1;
        }

        char tb[64]; int t = 0;
        for (const char *p = "Build"; *p; p++) tb[t++] = *p;
        if (hasfile) { tb[t++] = ' '; tb[t++] = '-'; tb[t++] = ' '; for (int i = 0; fname[i] && t < 60; i++) tb[t++] = fname[i]; if (modified) tb[t++] = '*'; }
        tb[t] = 0; vga_titlebar(tb);

        vga_clear();
        draw_sep();
        draw_sidebar(focus == F_SIDEBAR, sel, sbtop);
        draw_editor(focus == F_EDITOR, top, coloff, lang);
        set_status(focus, lang);
        if (focus == F_EDITOR && hasfile) {
            int cl, cc; cursor_lc(&cl, &cc); int sr = cl - top, sc = cc - coloff;
            if (sr >= 0 && sr < EROWS && sc >= 0 && sc < TXW) vga_setcursor(TX0 + sc, 1 + sr); else vga_setcursor(0, 24);
        } else vga_setcursor(0, 24);

        char c = keyboard_getc();
        if (focus == F_SIDEBAR) {
            if (c == K_ESC || c == K_QUIT) { if (confirm_quit()) quit = 1; }
            else if (c == K_UP)   { if (sel > 0) sel--; }
            else if (c == K_DOWN) { if (sel < li_n - 1) sel++; }
            else if (c == K_BKSP) { if (strcmp(sbdir, "/")) { char p[FS_NAME_LEN]; bp_parent(sbdir, p); strcpy(sbdir, p); sel = 0; } }
            else if (c == K_TAB)  { if (hasfile) focus = F_EDITOR; }
            else if (c == '\n') {
                if (li_n) {
                    if (li_dir[sel]) { strcpy(sbdir, li_path[sel]); sel = 0; }
                    else { load_file(li_path[sel]); lang = lang_of(fname); top = coloff = 0; focus = F_EDITOR; }
                }
            }
            else if (c == 'n' || c == 'N') { char nm[FS_NAME_LEN]; if (prompt_line("New file: ", nm, FS_NAME_LEN)) { char fp[FS_NAME_LEN]; bp_join(sbdir, nm, fp); fs_create(fp); load_file(fp); lang = lang_of(fname); top = coloff = 0; focus = F_EDITOR; } }
            else if (c == 'f' || c == 'F') { char nm[FS_NAME_LEN]; if (prompt_line("New folder: ", nm, FS_NAME_LEN)) { char fp[FS_NAME_LEN]; bp_join(sbdir, nm, fp); fs_mkdir(fp); } }
            else if (c == 'd' || c == 'D') { if (li_n && strcmp(li_name[sel], "..")) { fs_delete(li_path[sel]); if (sel > 0) sel--; } }
        } else { /* F_EDITOR */
            if (c == K_ESC) { focus = F_SIDEBAR; }
            else if (c == K_QUIT) { if (confirm_quit()) quit = 1; }
            else if (c == K_SAVE) { fs_write(fname, buf, (size_t)len); modified = 0; }
            else if (c == K_LEFT) { if (cur > 0) cur--; }
            else if (c == K_RIGHT){ if (cur < len) cur++; }
            else if (c == K_UP)   { int l,cl; cursor_lc(&l,&cl); if (l>0) cur=index_lc(l-1,cl); }
            else if (c == K_DOWN) { int l,cl; cursor_lc(&l,&cl); if (l<total_lines()-1) cur=index_lc(l+1,cl); }
            else if (c == K_HOME) { int l,cl; cursor_lc(&l,&cl); cur=index_lc(l,0); }
            else if (c == K_END)  { int l,cl; cursor_lc(&l,&cl); cur=line_end(l); }
            else if (c == K_PGUP) { int l,cl; cursor_lc(&l,&cl); cur=index_lc(l-EROWS,cl); }
            else if (c == K_PGDN) { int l,cl; cursor_lc(&l,&cl); int m=total_lines()-1,nl=l+EROWS; if(nl>m)nl=m; cur=index_lc(nl,cl); }
            else if (c == K_DEL)  del_forward();
            else if (c == K_BKSP) backspace();
            else if (c == K_TAB)  { for (int i = 0; i < 4; i++) insert(' '); }
            else if (c == K_GOTO) { char q[8]; if (prompt_line("Go to line: ", q, 8)) { int ln=0; for (int i=0;q[i]>='0'&&q[i]<='9';i++) ln=ln*10+(q[i]-'0'); if (ln>0) cur=line_start(ln-1); } }
            else if (c == K_FIND) { char q[48]; if (prompt_line("Find: ", q, 48)) { int p=find_from(q,cur+1); if(p<0)p=find_from(q,0); if(p>=0)cur=p; } }
            else if (c == '\n' || c == '\r') insert('\n');
            else if (c >= 32 && c < 127) insert(c);
        }
    }

    vga_titlebar("HisokaOS 0.2");
    vga_statusbar("help  commands  cls       HisokaOS 0.2  i386");
    vga_clear();
    vga_setcolor(VGA_LCYAN, VGA_BLACK); kputs("Build closed.\n"); vga_setcolor(VGA_LGREY, VGA_BLACK);
}
