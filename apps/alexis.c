/* alexis.c - Alexis, the HisokaOS assistant (native terminal UI).
 *
 * HisokaOS cannot run an LLM in its kernel, so - exactly like the Chromium browser -
 * the model (Qwen 2.5 Coder 1.5B, served as 'alexis' via Ollama) runs on the HOST and
 * is streamed here over our TCP/IP stack (10.0.2.2:8091, from alexis-server.py). This
 * file is the native front end: a purple banner, the > User / > Alexis transcript,
 * Alexis's private thoughts shown in grey, a box loader, and a set of / commands. */
#include "alexis.h"
#include "vga.h"
#include "keyboard.h"
#include "printf.h"
#include "net.h"
#include "rtl8139.h"
#include "pit.h"
#include "string.h"
#include "types.h"

#define A_HOST "10.0.2.2"
#define A_PORT 8091
#define A_BUF  16384
static char rbuf[A_BUF];

static int is_safe(char c) { return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.'; }
static void url_encode(const char *s, char *o, int max) {
    const char *h = "0123456789ABCDEF"; int k = 0;
    for (; *s && k < max - 4; s++) {
        char c = *s;
        if (is_safe(c)) o[k++] = c;
        else if (c == ' ') o[k++] = '+';
        else { o[k++] = '%'; o[k++] = h[(uint8_t)c >> 4]; o[k++] = h[(uint8_t)c & 15]; }
    }
    o[k] = 0;
}
static int find_sub(const char *hay, int n, const char *needle, int from) {
    int nl = (int)strlen(needle);
    for (int i = from; i + nl <= n; i++) { int k = 0; while (k < nl && hay[i+k] == needle[k]) k++; if (k == nl) return i; }
    return -1;
}

static void banner(void) {
    vga_clear();
    vga_setcolor(VGA_LMAGENTA, VGA_BLACK);
    kputs("    /\\   |    ___ \\ \\/ /  _  ___\n");
    kputs("   /  \\  |   | __  \\  /  | |/ __|\n");
    kputs("  / /\\ \\ |   | _|  /  \\  | |\\__ \\\n");
    kputs(" /_/  \\_\\|___|___|/_/\\_\\ |_||___/   ALEXIS\n");
    vga_setcolor(VGA_DGREY, VGA_BLACK);
    kputs("  /help   /quit\n\n");
    vga_setcolor(VGA_LGREY, VGA_BLACK);
}

/* an animated "thinking . .. ..." indicator on the current line */
static void thinking_anim(void) {
    int sx, sy; vga_getcursor(&sx, &sy);
    const char *dots[] = { ".  ", ".. ", "...", "   " };
    for (int f = 0; f < 9; f++) {
        const char *t = "  thinking ";
        for (int i = 0; t[i]; i++) vga_cell(sx + i, sy, t[i], VGA_DGREY, VGA_BLACK);
        for (int i = 0; dots[f & 3][i]; i++) vga_cell(sx + 11 + i, sy, dots[f & 3][i], VGA_LMAGENTA, VGA_BLACK);
        uint32_t s = pit_ticks(); while (pit_ticks() - s < 6) __asm__ volatile("hlt");
    }
    for (int x = sx; x <= 78; x++) vga_cell(x, sy, ' ', VGA_LGREY, VGA_BLACK);
    vga_setcursor(sx, sy);
}

/* print text grey, word by word with a small pause - Alexis "actively thinking" */
static void print_think_anim(const char *s, int n) {
    vga_setcolor(VGA_DGREY, VGA_BLACK);
    kputs("  "); int col = 2;
    for (int i = 0; i < n; i++) {
        char c = s[i];
        if (c == '\n') { kputs("\n  "); col = 2; continue; }
        char b[2] = { c, 0 }; kputs(b);
        if (++col >= 76) { kputs("\n  "); col = 2; }
        if (c == ' ') { uint32_t t = pit_ticks(); while (pit_ticks() - t < 4) __asm__ volatile("hlt"); }
    }
    kputs("\n");
    vga_setcolor(VGA_LGREY, VGA_BLACK);
}

/* read a (possibly multi-line) message; Shift+Enter inserts a newline, Enter sends.
 * Tracks the echo column per line so it never bleeds past the border. */
static int read_line(char *buf, int max) {
    int len = 0; buf[0] = 0; int col, sy; { int sx; vga_getcursor(&sx, &sy); col = sx; }
    for (;;) {
        char c = keyboard_getc();
        if (c == '\n' || c == '\r') { buf[len] = 0; kputs("\n"); return len; }
        else if (c == 0x0F) { if (len < max-1) { buf[len++] = '\n'; kputs("\n  "); int sx; vga_getcursor(&sx, &sy); col = sx; } }  /* Shift+Enter */
        else if (c == '\b') { if (len && buf[len-1] != '\n' && col > 2) { len--; col--; vga_cell(col, sy, ' ', VGA_WHITE, VGA_BLACK); vga_setcursor(col, sy); } }
        else if (c == 27) { buf[0] = 0; kputs("\n"); return 0; }
        else if (c >= 32 && c < 127 && len < max-1 && col < 77) { buf[len++] = c; vga_cell(col, sy, c, VGA_WHITE, VGA_BLACK); col++; vga_setcursor(col, sy); }
    }
}

/* print a block of text in a color, wrapping inside the content window */
static void print_wrapped(const char *s, int n, uint8_t fg, int indent) {
    vga_setcolor(fg, VGA_BLACK);
    for (int i = 0; i < indent; i++) kputs(" ");
    int col = indent;
    for (int i = 0; i < n; i++) {
        char c = s[i];
        if (c == '\n') { kputs("\n"); for (int k = 0; k < indent; k++) kputs(" "); col = indent; continue; }
        char b[2] = { c, 0 }; kputs(b);
        if (++col >= 77) { kputs("\n"); for (int k = 0; k < indent; k++) kputs(" "); col = indent; }
    }
    kputs("\n");
    vga_setcolor(VGA_LGREY, VGA_BLACK);
}

static void slash_help(void) {
    vga_setcolor(VGA_LCYAN, VGA_BLACK); kputs("  Alexis commands\n"); vga_setcolor(VGA_LGREY, VGA_BLACK);
    kputs("  /help     this list            /clear    clear the chat\n");
    kputs("  /quit     leave Alexis         /os       what Alexis knows about HisokaOS\n");
    kputs("  /who      who Alexis is        /think    explain the grey thought lines\n");
    kputs("  /code     ask for code help    /retry    (just ask again)\n\n");
}

void alexis_run(void) {
    banner();
    if (!rtl8139_present()) {
        vga_setcolor(VGA_LRED, VGA_BLACK);
        kputs("  Alexis needs the network (no adapter found).\n");
        vga_setcolor(VGA_DGREY, VGA_BLACK);
        kputs("  press any key...\n"); vga_setcolor(VGA_LGREY, VGA_BLACK);
        keyboard_getc(); vga_clear(); return;
    }

    char line[256], enc[1024], path[1100];
    for (;;) {
        vga_setcolor(VGA_LMAGENTA, VGA_BLACK); kputs("> User\n");
        vga_setcolor(VGA_WHITE, VGA_BLACK);    kputs("  ");
        int n = read_line(line, sizeof line);
        if (n == 0) continue;

        if (line[0] == '/') {
            if      (!strcmp(line, "/quit") || !strcmp(line, "/exit") || !strcmp(line, "/q")) break;
            else if (!strcmp(line, "/clear") || !strcmp(line, "/new")) { banner(); continue; }
            else if (!strcmp(line, "/help")) { slash_help(); continue; }
            else if (!strcmp(line, "/think")) { vga_setcolor(VGA_DGREY, VGA_BLACK); kputs("  Grey lines are Alexis's private reasoning - not addressed to you.\n\n"); vga_setcolor(VGA_LGREY, VGA_BLACK); continue; }
            else if (!strcmp(line, "/who"))  strcpy(line, "Who are you?");
            else if (!strcmp(line, "/os"))   strcpy(line, "Briefly, what is HisokaOS and what can it do?");
            else if (!strcmp(line, "/code")) strcpy(line, "Help me write a small program. Ask me what I want.");
            else { vga_setcolor(VGA_DGREY, VGA_BLACK); kputs("  unknown command - try /help\n\n"); vga_setcolor(VGA_LGREY, VGA_BLACK); continue; }
        }

        vga_setcolor(VGA_LMAGENTA, VGA_BLACK); kputs("> Alexis\n"); vga_setcolor(VGA_LGREY, VGA_BLACK);
        thinking_anim();

        url_encode(line, enc, sizeof enc);
        int k = 0; for (const char *p = "/chat?msg="; *p; p++) path[k++] = *p;
        for (int i = 0; enc[i] && k < (int)sizeof(path)-1; i++) path[k++] = enc[i];
        path[k] = 0;

        int len = net_http_get_ep(A_HOST, A_PORT, path, rbuf, A_BUF);
        if (len <= 0) {
            vga_setcolor(VGA_LRED, VGA_BLACK);
            kputs("  Alexis is offline. Start the host helper:  python3 alexis-server.py\n\n");
            vga_setcolor(VGA_LGREY, VGA_BLACK);
            continue;
        }
        int body = find_sub(rbuf, len, "\r\n\r\n", 0);
        body = body < 0 ? 0 : body + 4;
        int ti = find_sub(rbuf, len, "<<THINK>>", body);
        int si = find_sub(rbuf, len, "<<SAY>>", body);
        int ei = find_sub(rbuf, len, "<<END>>", body);
        if (si < 0) {  /* no structure - print raw body */
            print_wrapped(rbuf + body, len - body, VGA_LGREY, 2);
            kputs("\n"); continue;
        }
        if (ti >= 0 && si > ti) {
            int ts = ti + 9, tn = si - ts;
            if (tn > 0) print_think_anim(rbuf + ts, tn);   /* thoughts: grey, word by word */
        }
        int ss = si + 7, sn = (ei > ss ? ei : len) - ss;
        print_wrapped(rbuf + ss, sn, VGA_WHITE, 2);                    /* the answer */
        kputs("\n");
    }
    vga_statusbar("help  commands  man       HisokaOS 0.2  i386");
    vga_clear();
    vga_setcolor(VGA_LMAGENTA, VGA_BLACK); kputs("Alexis: goodbye.\n"); vga_setcolor(VGA_LGREY, VGA_BLACK);
}
