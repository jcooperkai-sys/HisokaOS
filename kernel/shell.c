/* shell.c - the HisokaOS command line. Reads a line, splits command/args, and
 * dispatches to built-ins covering system info, memory, the clock, security, and
 * the in-memory filesystem. Keeps a small command history. */
#include "shell.h"
#include "keyboard.h"
#include "mouse.h"
#include "printf.h"
#include "vga.h"
#include "string.h"
#include "pmm.h"
#include "pit.h"
#include "heap.h"
#include "ports.h"
#include "ramfs.h"
#include "rtc.h"
#include "paging.h"
#include "pci.h"
#include "rtl8139.h"
#include "net.h"
#include "ata.h"
#include "persist.h"
#include "klog.h"
#include "task.h"
#include "gfx.h"
#include "browser.h"
#include "screens.h"
#include "snake.h"
#include "g2048.h"
#include "ttt.h"
#include "edit.h"
#include "explorer.h"
#include "settings.h"
#include "draw.h"
#include "clock.h"
#include "tetris.h"
#include "version.h"
#include "agentinfo.h"
#include "build.h"
#include "imgview.h"
#include "alexis.h"
#include "tools.h"
#include "ui.h"
#include "procfs.h"
#include "media.h"
#include "tools3.h"
#include "tools4.h"
#include "tools5.h"
#include "tools6.h"
#include "tools7.h"
#include "tools8.h"
#include "speaker.h"
#include "piano.h"
#include "backup.h"
#include "forum.h"

#define LINE 128
#define HIST 16

static char hist[HIST][LINE];
static int  hist_n;

static char cwd[FS_NAME_LEN] = "/home";   /* current working directory */

static void parent_dir(const char *path, char *out) {
    int last = 0, n = (int)strlen(path);
    for (int i = 0; i < n; i++) if (path[i] == '/') last = i;
    if (last == 0) strcpy(out, "/"); else { memcpy(out, path, last); out[last] = 0; }
}
static const char *base_name(const char *path) {
    const char *b = path; for (const char *p = path; *p; p++) if (*p == '/') b = p + 1; return b;
}
/* resolve `arg` against cwd into an absolute normalized path (handles . and ..) */
static void normalize(const char *arg, char *out) {
    char tmp[160]; int t = 0;
    if (!arg || !*arg) { strcpy(out, cwd); return; }
    if (arg[0] == '/') { while (arg[t] && t < 159) { tmp[t] = arg[t]; t++; } tmp[t] = 0; }
    else {
        int i = 0; while (cwd[i] && t < 159) tmp[t++] = cwd[i++];
        if (t && tmp[t-1] != '/' && t < 159) tmp[t++] = '/';
        i = 0; while (arg[i] && t < 159) tmp[t++] = arg[i++];
        tmp[t] = 0;
    }
    char comps[20][FS_NAME_LEN]; int nc = 0;
    char cur[FS_NAME_LEN]; int ci = 0;
    for (const char *p = tmp;; p++) {
        char c = *p;
        if (c == '/' || c == 0) {
            cur[ci] = 0;
            if (ci > 0) {
                if      (!strcmp(cur, ".")) { }
                else if (!strcmp(cur, "..")) { if (nc > 0) nc--; }
                else if (nc < 20) strcpy(comps[nc++], cur);
            }
            ci = 0;
            if (c == 0) break;
        } else if (ci < FS_NAME_LEN - 1) cur[ci++] = c;
    }
    if (nc == 0) { strcpy(out, "/"); return; }
    int o = 0;
    for (int i = 0; i < nc && o < FS_NAME_LEN - 2; i++) {
        out[o++] = '/';
        for (int k = 0; comps[i][k] && o < FS_NAME_LEN - 1; k++) out[o++] = comps[i][k];
    }
    out[o] = 0;
}
/* normalize into rotating buffers so two paths can be resolved in one expression */
static const char *P(const char *arg) {
    static char buf[3][FS_NAME_LEN]; static int bi = 0;
    char *out = buf[bi]; bi = (bi + 1) % 3; normalize(arg, out); return out;
}

/* config helpers are defined lower down (next to the setup wizard) but used here */
static int  cfg_get(const char *key, char *out, int max);
static void cfg_hostname(char *out, int max);

static void prompt(void) {
    char hn[40]; cfg_hostname(hn, sizeof hn);
    vga_setcolor(VGA_LGREEN, VGA_BLACK); kputs(hn);
    vga_setcolor(VGA_DGREY,  VGA_BLACK); kputs(":");
    vga_setcolor(VGA_LCYAN,  VGA_BLACK); kputs(cwd);
    vga_setcolor(VGA_LGREY,  VGA_BLACK); kputs("> ");
}

/* every command name, for Tab-completion */
static const char *CMDS[] = {
    "help","commands","clear","cls","cpuid","calc","cal","ascii","rand","poweroff",
    "cp","mv","wc","mem","vm","pgfault","sec","lspci","net","arp","ping","dns","nslookup","fetch","curl","snake","2048","ttt",
    "edit","date","uname","about","uptime","reboot","panic","echo","ls","cat",
    "touch","rm","stat","write","append","history","grep","find","head","tail",
    "rev","upper","lower","seq",
    "man","motd","keys","apps","menu","home","start","explorer","files","settings","control","draw","paint","clock",
    "pwd","whoami","hostname","env","yes","factor",
    "basename","dirname","tac","nl","sort","uniq","hexdump","xxd","du","df","free","sleep","sync",
    "flip","roll","dice","len","hex","bin","ord","chr","box","cowsay","repeat","file","run","sh",
    "tetris","calculator","cd","mkdir","rmdir","tree","log","dmesg","ps","multitask","threads",
    "browse","chromium","gfx","shutdown","restart","bootscreen","shutdownscreen",
    "restartscreen","panicscreen","panictest",
    "update","upgrade","setup","reset","factory-reset","agent","agentinfo","whatami",
    "more","less","open","download","wget","which","realpath","id","build","code",
    "true","false","arch","nproc","printf","expr","base64","sum",
    "img","view","image","fcv","convert","alexis","ai","ask",
    "todo","monitor","top","units","stopwatch","base","passgen","tools",
    "calendar","contacts","bookmarks","timer","counter","expenses",
    "changelog","whatsnew","logs","media","player",
    "worldclock","nettools","diskusage","kanban","notes",
    "finance","tip","bmi","habits",
    "datecalc","roman","cipher","morse","primes",
    "percent","textstats","num2words","pomodoro","timestable",
    "gcd","factorial","fibonacci","reverse","lorem","eightball","discount",
    "currency","savings","colorpick","color","binclock","piglatin","water","leet",
    "beep","piano","mouse","backup","backups",
    "strings","od","fold","cksum","cut","groups","tty","printenv","dir","vdir","egrep","fgrep","type", 0
};

/* redraw the input line at (sx,sy) and place the cursor at column sx+pos */
static void rl_draw(const char *buf, int len, int pos, int sx, int sy, int *lastlen) {
    for (int k = 0; k < len && sx + k < 79; k++) vga_cell(sx + k, sy, buf[k], VGA_LGREY, VGA_BLACK);
    for (int k = len; k < *lastlen && sx + k < 79; k++) vga_cell(sx + k, sy, ' ', VGA_LGREY, VGA_BLACK);
    *lastlen = len;
    int cx = sx + pos; if (cx > 78) cx = 78;
    vga_setcursor(cx, sy);
}

/* readline with full line editing + Linux/readline keyboard shortcuts:
 *   Left/Ctrl-B  Right/Ctrl-F  Ctrl-A home  Ctrl-E end  Backspace  Ctrl-D delete
 *   Ctrl-U kill line  Ctrl-K kill-to-end  Ctrl-W delete-word  Ctrl-L clear
 *   Ctrl-C cancel  Up/Ctrl-P + Down/Ctrl-N history  Tab complete */
static void readline(char *buf) {
    int len = 0, pos = 0, hidx = hist_n, lastlen = 0;
    int sx, sy; vga_getcursor(&sx, &sy);
    buf[0] = 0;
    for (;;) {
        char c = keyboard_getc();
        if (c == '\n' || c == '\r') { buf[len] = 0; vga_setcursor(sx + len, sy); kputs("\n"); return; }
        else if (c == 0x03) { buf[0] = 0; vga_setcursor(sx + lastlen, sy); kputs("^C\n"); return; }  /* Ctrl-C */
        else if (c == '\b')  { if (pos > 0) { for (int k = pos-1; k < len-1; k++) buf[k] = buf[k+1]; len--; pos--; } }
        else if (c == 0x04)  { if (pos < len) { for (int k = pos; k < len-1; k++) buf[k] = buf[k+1]; len--; } }  /* Ctrl-D */
        else if (c == 0x01)  pos = 0;            /* Ctrl-A: home */
        else if (c == 0x05)  pos = len;          /* Ctrl-E: end  */
        else if (c == 0x02)  { if (pos > 0)   pos--; }   /* Left / Ctrl-B */
        else if (c == 0x06)  { if (pos < len) pos++; }   /* Right / Ctrl-F */
        else if (c == 0x15)  { len = 0; pos = 0; }       /* Ctrl-U: kill line */
        else if (c == 0x0B)  { len = pos; }              /* Ctrl-K: kill to end */
        else if (c == 0x17)  {                            /* Ctrl-W: delete word */
            int e = pos;
            while (pos > 0 && buf[pos-1] == ' ') pos--;
            while (pos > 0 && buf[pos-1] != ' ') pos--;
            int del = e - pos;
            for (int k = pos; k + del < len; k++) buf[k] = buf[k+del];
            len -= del;
        }
        else if (c == 0x0C)  { vga_clear(); prompt(); vga_getcursor(&sx, &sy); lastlen = 0; }  /* Ctrl-L */
        else if (c == 0x10)  { if (hist_n && hidx > 0) { hidx--; strcpy(buf, hist[hidx]); len = (int)strlen(buf); pos = len; } } /* up */
        else if (c == 0x0E)  { if (hidx < hist_n) { hidx++; if (hidx == hist_n) { buf[0]=0; len=0; } else { strcpy(buf, hist[hidx]); len=(int)strlen(buf); } pos = len; } } /* down */
        else if (c == '\t')  {                            /* Tab: complete a command */
            buf[len] = 0; const char *m = 0; int nm = 0;
            for (int k = 0; CMDS[k]; k++) if (!strncmp(CMDS[k], buf, (size_t)len)) { m = CMDS[k]; nm++; }
            if (nm == 1 && m) { int ml = (int)strlen(m); for (int k = len; k < ml && len < LINE-1; k++) buf[len++] = m[k]; pos = len; }
        }
        else if (c >= 32 && c < 127) {                    /* insert at cursor */
            if (len < LINE-1) { for (int k = len; k > pos; k--) buf[k] = buf[k-1]; buf[pos++] = c; len++; }
        }
        rl_draw(buf, len, pos, sx, sy, &lastlen);
    }
}

/* split "cmd rest..." -> returns rest pointer (after first space), null-terminates cmd */
static char *split(char *line) {
    char *p = line;
    while (*p && *p != ' ') p++;
    if (!*p) return p;          /* no args -> points at '\0' */
    *p++ = 0;
    while (*p == ' ') p++;
    return p;
}

static const char *months[] = { "???","Jan","Feb","Mar","Apr","May","Jun",
                                "Jul","Aug","Sep","Oct","Nov","Dec" };
static void p2(uint8_t v) { kprintf("%s%u", v < 10 ? "0" : "", v); }

/* a full-screen pager: shows long text one page at a time, scrollable with
 * Space (page down), b (page up), Up/Down (one line), q to quit. */
static void pager(const char *text) {
    static int starts[1200];
    int len = (int)strlen(text), nl = 0;
    starts[nl++] = 0;
    for (int i = 0; i < len && nl < 1199; i++) if (text[i] == '\n') starts[nl++] = i + 1;
    const int PAGE = 22;            /* content rows 1..22 (row 0 = title, 23/24 = chrome) */
    int top = 0;
    for (;;) {
        vga_clear();
        vga_setcolor(VGA_LGREY, VGA_BLACK);
        for (int r = 0; r < PAGE; r++) {
            int ln = top + r; if (ln >= nl) break;
            int s = starts[ln];
            int e = (ln + 1 < nl) ? starts[ln + 1] - 1 : len;
            int y = 1 + r;          /* start at the first content row, never the title bar */
            for (int x = 1, i = s; i < e && x <= 78; i++, x++) vga_cell(x, y, text[i], VGA_LGREY, VGA_BLACK);
        }
        int pages = (nl + PAGE - 1) / PAGE, cur = top / PAGE + 1;
        char st[72]; int o = 0;
        for (const char *a = " Space: next   b: back   Up/Dn: line   q: quit     page "; *a; a++) st[o++] = *a;
        char num[8]; int m, v;
        m = 0; v = cur;   if (!v) num[m++]='0'; while (v){num[m++]=(char)('0'+v%10);v/=10;} while (m) st[o++]=num[--m];
        st[o++] = '/';
        m = 0; v = pages; if (!v) num[m++]='0'; while (v){num[m++]=(char)('0'+v%10);v/=10;} while (m) st[o++]=num[--m];
        st[o] = 0;
        vga_statusbar(st);
        vga_setcursor(1, 23);
        char c = keyboard_getc();
        if      (c == 'q' || c == 27)      break;
        else if (c == ' ' || c == '\n')    { if (top + PAGE < nl) top += PAGE; }
        else if (c == 'b')                 { top -= PAGE; if (top < 0) top = 0; }
        else if (c == 0x0E)                { if (top + 1 < nl) top++; }   /* Down: one line */
        else if (c == 0x10)                { if (top > 0) top--; }        /* Up: one line   */
    }
    vga_clear();
}

/* the complete, grouped command reference - shown by 'help', scrollable via pager */
static const char HELP_FULL[] =
"HisokaOS - all commands  (Space/b to page, Up/Dn line, q to quit)\n"
"================================================================\n"
"\n"
"GETTING AROUND\n"
"  menu / home / start    open the Home screen (arrow keys)\n"
"  help                   this list (scrollable)\n"
"  commands               quick flat list of every command\n"
"  man                    the full manual (scrollable)\n"
"  keys                   keyboard shortcuts\n"
"  clear / cls            clear the screen\n"
"\n"
"FILES & FOLDERS\n"
"  ls [dir]   cd <dir>   pwd   tree [dir]\n"
"  cat <f>    more/less <f>   head <f>   tail <f>   tac <f>   nl <f>\n"
"  build / code / edit <f>   the Build code editor (syntax, line numbers, find)\n"
"  write <f> <text>   append <f> <text>   touch <f>\n"
"  mkdir <d>  rmdir <d>   rm <f>   cp <a> <b>   mv <a> <b>\n"
"  stat <f>   wc <f>   file <f>   hexdump/xxd <f>   sort/uniq <f>\n"
"  grep <pat> <f>    find <pat>    explorer / files (visual browser)\n"
"  open <file|app>    launch an app, run a .sh script, or edit a file\n"
"  which <name>   realpath <path>   du   df   sync (save to disk)\n"
"  apps live in /Applications - open Files and press Enter to launch one\n"
"\n"
"SYSTEM\n"
"  uname  about  uptime  mem  free  vm  pgfault  cpuid  lspci\n"
"  update [apply]         check / install the system revision\n"
"  setup                  re-run the setup wizard\n"
"  reset [confirm]        factory reset (wipes the disk)\n"
"  shutdown / poweroff    power off       restart / reboot   restart\n"
"  bootscreen  shutdownscreen  restartscreen  panicscreen   (previews)\n"
"  panictest              trigger a real kernel-panic screen\n"
"  log   dmesg            system log / boot messages\n"
"  ps   multitask / threads   (scheduler demo)\n"
"\n"
"AGENT (tell AI agents what this OS is)\n"
"  agent / agentinfo      machine-readable descriptor -> /System/agent.json\n"
"  whatami                print the descriptor without saving it\n"
"\n"
"NETWORK\n"
"  net   arp   ping <host>   dns <name>   fetch / curl <url>\n"
"  download / wget <url> [dest]   save a file from the web into a folder\n"
"  browse <url>           streamed graphical page (needs the host helper)\n"
"\n"
"APPS & GAMES\n"
"  settings / control     Control Center (theme + info)\n"
"  draw / paint   clock   calculator   gfx (graphics demo)\n"
"  snake   2048   tetris   ttt\n"
"\n"
"TOOLS & FUN\n"
"  calc <expr>  cal  date  ascii  rand  seq  rev  upper  lower  echo\n"
"  sleep <s>  factor <n>  basename  dirname  flip  roll  len  hex  bin\n"
"  ord  chr  box  cowsay  repeat  yes  run / sh <script>\n"
"  pwd  whoami  hostname  env\n"
"\n"
"TIP: type the first letters of a command and press Tab to complete it.\n";

static void cmd_help(void) { pager(HELP_FULL); }

static void cmd_mem(void) {
    uint32_t tot = pmm_total_frames(), fr = pmm_free_frames();
    kprintf("RAM   : %u MiB total, %u MiB free  (%u/%u frames used)\n",
            tot * 4 / 1024, fr * 4 / 1024, tot - fr, tot);
    kprintf("Heap  : %u bytes in use\n", (uint32_t)heap_used());
    kprintf("Files : %u in ramfs\n", fs_count());
}

static void cmd_vm(void) {
    vga_setcolor(VGA_LCYAN, VGA_BLACK); kputs("HisokaOS virtual memory:\n");
    vga_setcolor(VGA_LGREY, VGA_BLACK);
    kprintf("  paging  : %s\n", paging_enabled() ? "ENABLED (MMU on)" : "off");
    kputs("  scheme  : 4 MiB pages (PSE), single kernel page directory\n");
    kprintf("  mapped  : identity 0x00000000 - %p  (%u MiB)\n",
            (void *)(paging_mapped_mb() * 0x100000u), paging_mapped_mb());
    kputs("  faults  : trapped by #PF handler - reports CR2, contains the crash\n");
    kputs("  hint    : run 'pgfault' to watch the handler catch a bad access\n");
}

static void cmd_lspci(void) {
    vga_setcolor(VGA_LCYAN, VGA_BLACK); kputs("PCI devices:\n");
    vga_setcolor(VGA_LGREY, VGA_BLACK);
    for (uint32_t i = 0; i < pci_count(); i++) {
        pci_device_t *d = pci_get(i);
        kprintf("  %u:%u.%u  %x:%x  %s - %s\n",
                d->bus, d->slot, d->func, d->vendor, d->device, d->vendor_name, d->class_name);
    }
    if (!pci_count()) kputs("  (none found)\n");
}

static void hx2(uint8_t v) { const char *h = "0123456789abcdef"; char s[3] = { h[v >> 4], h[v & 15], 0 }; kputs(s); }

static void cmd_net(void) {
    vga_setcolor(VGA_LCYAN, VGA_BLACK); kputs("Network\n");
    vga_setcolor(VGA_LGREY, VGA_BLACK);
    pci_device_t *nic = pci_find(0x10EC, 0x8139);   /* RTL8139 */
    if (nic && rtl8139_present()) {
        const uint8_t *m = rtl8139_mac();
        kprintf("  device    : Realtek RTL8139 Ethernet (%x:%x)\n", nic->vendor, nic->device);
        kprintf("  location  : PCI %u:%u.%u\n", nic->bus, nic->slot, nic->func);
        kputs("  driver    : loaded\n");
        kputs("  mac       : ");
        for (int i = 0; i < 6; i++) { hx2(m[i]); if (i < 5) kputs(":"); }
        kputs("\n");
        const uint8_t *ip = net_my_ip(), *gw = net_gw_ip();
        kprintf("  address   : %u.%u.%u.%u\n", ip[0], ip[1], ip[2], ip[3]);
        kprintf("  gateway   : %u.%u.%u.%u\n", gw[0], gw[1], gw[2], gw[3]);
        kputs("  link      : up (ARP working - try 'arp' or 'arp <ip>')\n");
    } else if (nic) {
        kputs("  device    : Realtek RTL8139 Ethernet (driver failed to init)\n");
    } else {
        kputs("  device    : no network controller detected\n");
    }
    kputs("  next      : IP + ICMP (ping), then DNS, TCP, HTTP\n");
}

static void cmd_arp(const char *args) {
    if (!rtl8139_present()) { kputs("arp: no network device\n"); return; }
    uint8_t ip[4];
    if (*args) { if (!net_parse_ip(args, ip)) { kputs("arp: bad IP (use a.b.c.d)\n"); return; } }
    else for (int i = 0; i < 4; i++) ip[i] = net_gw_ip()[i];

    kprintf("arp: who has %u.%u.%u.%u ?\n", ip[0], ip[1], ip[2], ip[3]);
    uint8_t mac[6];
    if (net_arp_resolve(ip, mac)) {
        kprintf("  %u.%u.%u.%u is at ", ip[0], ip[1], ip[2], ip[3]);
        for (int i = 0; i < 6; i++) { hx2(mac[i]); if (i < 5) kputs(":"); }
        kputs("\n");
    } else {
        kputs("  no reply (timeout)\n");
    }
}

static void cmd_ping(const char *args) {
    if (!rtl8139_present()) { kputs("ping: no network device\n"); return; }
    uint8_t ip[4];
    if (*args) { if (!net_parse_ip(args, ip)) { kputs("ping: bad IP (use a.b.c.d)\n"); return; } }
    else for (int i = 0; i < 4; i++) ip[i] = net_gw_ip()[i];
    kprintf("ping %u.%u.%u.%u\n", ip[0], ip[1], ip[2], ip[3]);
    int got = 0;
    for (int seq = 1; seq <= 3; seq++) {
        uint32_t t0 = pit_ticks(); int ttl = 0;
        if (net_ping(ip, &ttl)) {
            uint32_t dt = (pit_ticks() - t0) * 10;
            kprintf("  reply from %u.%u.%u.%u: seq=%u ttl=%u time=%ums\n",
                    ip[0], ip[1], ip[2], ip[3], (uint32_t)seq, (uint32_t)ttl, dt);
            got++;
        } else {
            kprintf("  seq=%u request timed out\n", (uint32_t)seq);
        }
    }
    kprintf("  %u/3 replies\n", (uint32_t)got);
}

static void cmd_dns(const char *host) {
    if (!rtl8139_present()) { kputs("dns: no network device\n"); return; }
    if (!*host) { kputs("usage: dns <hostname>\n"); return; }
    kprintf("resolving %s ...\n", host);
    uint8_t ip[4];
    if (net_dns_resolve(host, ip))
        kprintf("  %s is %u.%u.%u.%u\n", host, ip[0], ip[1], ip[2], ip[3]);
    else
        kputs("  resolution failed (timeout)\n");
}

static int starts_with_ci(const char *s, const char *pat) {
    for (; *pat; s++, pat++) {
        char a = *s;   if (a >= 'A' && a <= 'Z') a += 32;
        char b = *pat; if (b >= 'A' && b <= 'Z') b += 32;
        if (a != b) return 0;
    }
    return 1;
}

/* render HTML as readable text (lynx-style): drop tags, skip script/style, decode
 * a few entities, collapse whitespace, mark links [n] inline and list them after. */
static char g_links[24][96];   /* links collected from the last rendered page */
static int  g_nlinks;
static char g_host[64];        /* host of the last page (for relative links) */

static void render_html(const char *p) {
    int lastsp = 1; g_nlinks = 0;
    while (*p) {
        if (*p == '<') {
            if (starts_with_ci(p, "<script") || starts_with_ci(p, "<style")) {
                const char *close = starts_with_ci(p, "<script") ? "</script" : "</style";
                p++; while (*p && !starts_with_ci(p, close)) p++;
                while (*p && *p != '>') p++; if (*p) p++;
                continue;
            }
            const char *t = p + 1; int closing = (*t == '/'); if (closing) t++;
            char tag[12]; int ti = 0;
            while (*t && *t != '>' && *t != ' ' && *t != '/' && ti < 11) {
                char c = *t; if (c >= 'A' && c <= 'Z') c += 32; tag[ti++] = c; t++;
            }
            tag[ti] = 0;
            if (!strcmp(tag, "a") && !closing) {
                const char *e = p; while (*e && *e != '>') e++;
                for (const char *r = p; r + 4 < e; r++)
                    if (starts_with_ci(r, "href")) {
                        const char *v = r + 4; while (*v && *v != '"' && *v != '\'') v++;
                        if (*v) { char q = *v++; int u = 0;
                            if (g_nlinks < 24) { while (*v && *v != q && u < 95) g_links[g_nlinks][u++] = *v++; g_links[g_nlinks][u] = 0; } }
                        break;
                    }
                if (g_nlinks < 24) { kprintf("[%u]", (uint32_t)(g_nlinks + 1)); g_nlinks++; lastsp = 0; }
            } else if (!strcmp(tag, "br") || !strcmp(tag, "p")  || !strcmp(tag, "div") ||
                       !strcmp(tag, "li") || !strcmp(tag, "tr") || !strcmp(tag, "h1")  ||
                       !strcmp(tag, "h2") || !strcmp(tag, "h3") || !strcmp(tag, "ul")) {
                if (!lastsp) { kputs("\n"); lastsp = 1; }
            }
            while (*p && *p != '>') p++; if (*p) p++;
            continue;
        }
        if (*p == '&') {
            if (starts_with_ci(p, "&amp;"))  { kputs("&");  p += 5; lastsp = 0; continue; }
            if (starts_with_ci(p, "&lt;"))   { kputs("<");  p += 4; lastsp = 0; continue; }
            if (starts_with_ci(p, "&gt;"))   { kputs(">");  p += 4; lastsp = 0; continue; }
            if (starts_with_ci(p, "&quot;")) { kputs("\""); p += 6; lastsp = 0; continue; }
            if (starts_with_ci(p, "&#39;"))  { kputs("'");  p += 5; lastsp = 0; continue; }
            if (starts_with_ci(p, "&nbsp;")) { kputs(" ");  p += 6; lastsp = 1; continue; }
        }
        char c = *p++;
        if (c == '\n' || c == '\r' || c == '\t') c = ' ';
        if (c == ' ') { if (!lastsp) { kputs(" "); lastsp = 1; } }
        else { char s[2] = { c, 0 }; kputs(s); lastsp = 0; }
    }
    if (g_nlinks) {
        kputs("\n\nLinks on this page:\n");
        for (int i = 0; i < g_nlinks; i++) kprintf("  [%u] %s\n", (uint32_t)(i + 1), g_links[i]);
    }
}

static int http_status(const char *body) {
    const char *p = body;
    while (*p && *p != ' ') p++;            /* skip "HTTP/1.x" */
    while (*p == ' ') p++;
    int code = 0; while (*p >= '0' && *p <= '9') { code = code*10 + (*p-'0'); p++; }
    return code;
}
static int find_header(const char *body, int n, const char *name, char *out, int max) {
    for (int i = 0; i < n; i++) {
        if (i == 0 || body[i-1] == '\n') {
            if (starts_with_ci(body + i, name)) {
                const char *v = body + i + (int)strlen(name);
                while (*v == ' ') v++;
                int o = 0; while (*v && *v != '\r' && *v != '\n' && o < max-1) out[o++] = *v++;
                out[o] = 0; return 1;
            }
        }
        if (i + 3 < n && body[i]=='\r' && body[i+1]=='\n' && body[i+2]=='\r' && body[i+3]=='\n') break;
    }
    return 0;
}
static void parse_url(const char *s, char *host, char *path) {
    if      (starts_with_ci(s, "http://"))  s += 7;
    else if (starts_with_ci(s, "https://")) s += 8;
    int h = 0; while (*s && *s != '/' && h < 63) host[h++] = *s++;
    host[h] = 0;
    if (*s == '/') { int p = 0; while (*s && p < 95) path[p++] = *s++; path[p] = 0; }
    else strcpy(path, "/");
}

static void cmd_fetch(const char *arg) {
    if (!rtl8139_present()) { kputs("fetch: no network device\n"); return; }
    if (!*arg) { kputs("usage: fetch <site>    e.g.  fetch example.com\n"); return; }
    static char body[8192];
    char host[64], path[96];
    parse_url(arg, host, path);
    for (int hop = 0; hop < 5; hop++) {
        kprintf("fetching http://%s%s ...\n", host, path);
        int n = net_http_get(host, path, body, sizeof(body));
        if (n <= 0) { kprintf("  fetch failed (error %d)\n", n); return; }
        int code = http_status(body);
        if (code >= 300 && code < 400) {                 /* a redirect */
            char loc[160];
            if (find_header(body, n, "location:", loc, sizeof(loc))) {
                if (starts_with_ci(loc, "https://")) {
                    kprintf("  %d redirect to %s\n", code, loc);
                    vga_setcolor(VGA_YELLOW, VGA_BLACK);
                    kputs("  This site requires HTTPS (encryption), which HisokaOS does not support yet.\n");
                    vga_setcolor(VGA_LGREY, VGA_BLACK);
                    kputs("  Try a plain-http site:  fetch neverssl.com   or   fetch info.cern.ch\n");
                    return;
                }
                if (loc[0] == '/') strcpy(path, loc);     /* relative redirect, same host */
                else parse_url(loc, host, path);          /* absolute http redirect */
                continue;
            }
            kprintf("  %d redirect (no Location header)\n", code); return;
        }
        vga_setcolor(VGA_DGREY, VGA_BLACK);               /* show the status line */
        for (int i = 0; i < n && body[i] != '\r' && body[i] != '\n'; i++) { char s[2] = { body[i], 0 }; kputs(s); }
        kputs("\n"); vga_setcolor(VGA_LGREY, VGA_BLACK);
        char *html = 0;
        for (int i = 0; i + 3 < n; i++)
            if (body[i]=='\r' && body[i+1]=='\n' && body[i+2]=='\r' && body[i+3]=='\n') { html = &body[i+4]; break; }
        strncpy(g_host, host, 63); g_host[63] = 0;        /* remember host for relative links */
        kputs("\n"); render_html(html ? html : body); kputs("\n");
        return;
    }
    kputs("  too many redirects\n");
}

static void cmd_date(void) {
    rtc_time_t t; rtc_now(&t);
    kprintf("%s %u %u  ", months[t.month <= 12 ? t.month : 0], t.day, t.year);
    p2(t.hour); kputs(":"); p2(t.min); kputs(":"); p2(t.sec); kputs(" (RTC)\n");
}

static void cmd_sec(void) {
    vga_setcolor(VGA_LCYAN, VGA_BLACK); kputs("Protection\n");
    vga_setcolor(VGA_LGREY, VGA_BLACK);
    kputs("  * ring 0 / ring 3 privilege separation (GDT)\n");
    kputs("  * virtual memory with page-fault isolation (paging)\n");
    kputs("  * CPU exceptions are trapped and reported\n");
    kputs("  planned: syscall gate, per-process address spaces, user accounts\n");
}

static void cmd_ls(const char *dir) {
    int shown = 0;
    for (int i = 0; i < FS_MAX_FILES; i++) {
        fs_file_t *f = fs_at(i);
        if (!f || !f->used) continue;
        char pd[FS_NAME_LEN]; parent_dir(f->name, pd);
        if (strcmp(pd, dir)) continue;
        if (f->is_dir) { vga_setcolor(VGA_LCYAN, VGA_BLACK); kprintf("  %s/\n", base_name(f->name)); vga_setcolor(VGA_LGREY, VGA_BLACK); }
        else kprintf("  %s\t%u bytes\n", base_name(f->name), (uint32_t)f->len);
        shown++;
    }
    if (!shown) kputs("  (empty)\n");
}
static void cmd_cd(const char *arg) {
    const char *p = (arg && *arg) ? P(arg) : "/home";
    if (fs_isdir(p)) strcpy(cwd, p);
    else kprintf("cd: %s: no such directory\n", p);
}
static void cmd_mkdir(const char *arg) {
    if (!*arg) { kputs("usage: mkdir <name>\n"); return; }
    const char *p = P(arg);
    if (fs_find(p)) kprintf("mkdir: %s already exists\n", p);
    else kprintf(fs_mkdir(p) == 0 ? "created %s\n" : "mkdir: failed\n", p);
}
static void cmd_rm_path(const char *path) {
    fs_file_t *f = fs_find(path);
    if (!f) { kprintf("rm: %s: not found\n", path); return; }
    if (f->is_dir) { kprintf("rm: %s is a directory (use rmdir)\n", path); return; }
    fs_delete(path); kprintf("removed %s\n", path);
}
static void cmd_rmdir(const char *path) {
    if (!fs_isdir(path) || !strcmp(path, "/")) { kprintf("rmdir: %s: not a directory\n", path); return; }
    for (int i = 0; i < FS_MAX_FILES; i++) {
        fs_file_t *f = fs_at(i);
        if (f && f->used) { char pd[FS_NAME_LEN]; parent_dir(f->name, pd); if (!strcmp(pd, path)) { kprintf("rmdir: %s: not empty\n", path); return; } }
    }
    fs_delete(path); kprintf("removed %s\n", path);
}
static void cmd_tree_at(const char *dir, int depth) {
    for (int i = 0; i < FS_MAX_FILES; i++) {
        fs_file_t *f = fs_at(i);
        if (!f || !f->used) continue;
        char pd[FS_NAME_LEN]; parent_dir(f->name, pd);
        if (strcmp(pd, dir)) continue;
        for (int d = 0; d <= depth; d++) kputs("  ");
        if (f->is_dir) { vga_setcolor(VGA_LCYAN, VGA_BLACK); kprintf("%s/\n", base_name(f->name)); vga_setcolor(VGA_LGREY, VGA_BLACK); cmd_tree_at(f->name, depth + 1); }
        else kprintf("%s\n", base_name(f->name));
    }
}
static void cmd_tree(const char *arg) {
    const char *root = (arg && *arg) ? P(arg) : cwd;
    vga_setcolor(VGA_LCYAN, VGA_BLACK); kprintf("%s\n", root); vga_setcolor(VGA_LGREY, VGA_BLACK);
    cmd_tree_at(root, 0);
}

/* a /proc path? then it's live - regenerate before reading */
static int is_proc_path(const char *p) { return p[0]=='/'&&p[1]=='p'&&p[2]=='r'&&p[3]=='o'&&p[4]=='c'&&(p[5]=='/'||p[5]==0); }

static void cmd_cat(const char *name) {
    if (is_proc_path(name)) procfs_refresh();
    fs_file_t *f = fs_find(name);
    if (!f) { kprintf("cat: %s: not found\n", name); return; }
    for (size_t i = 0; i < f->len; i++) { char s[2] = { (char)f->data[i], 0 }; kputs(s); }
    kputs("\n");
}


/* read real CPU identity straight from the cpuid instruction */
static void cmd_cpuid(void) {
    uint32_t a, b, c, d;
    char vendor[13];
    __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(0u));
    ((uint32_t *)vendor)[0] = b; ((uint32_t *)vendor)[1] = d; ((uint32_t *)vendor)[2] = c; vendor[12] = 0;
    kprintf("vendor   : %s\n", vendor);
    char brand[49]; uint32_t *bp = (uint32_t *)brand;
    for (uint32_t i = 0; i < 3; i++) {
        __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(0x80000002u + i));
        bp[i*4+0] = a; bp[i*4+1] = b; bp[i*4+2] = c; bp[i*4+3] = d;
    }
    brand[48] = 0;
    kprintf("brand    : %s\n", brand);
    __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(1u));
    kprintf("features :%s%s%s%s%s\n",
            (d & (1u<<0))  ? " fpu"  : "", (d & (1u<<4))  ? " tsc" : "",
            (d & (1u<<23)) ? " mmx"  : "", (d & (1u<<25)) ? " sse" : "",
            (c & (1u<<0))  ? " sse3" : "");
}

/* smart calculator: + - * / %, ^ (power), sqrt, parentheses, precedence, AND plain
 * English math words ("14 times 25", "512 divided by 2", "5 squared"). */
static const char *CALC;
static long calc_expr(void);
static void calc_ws(void) { while (*CALC == ' ') CALC++; }
static long ipow(long b, long e) { long r = 1; if (e < 0) return 0; while (e-- > 0) r *= b; return r; }
static long isqrt(long n) { if (n < 0) return 0; long r = 0; while ((r + 1) * (r + 1) <= n) r++; return r; }

static long calc_power(void);
static long calc_atom(void) {
    calc_ws();
    if (!strncmp(CALC, "sqrt", 4)) { CALC += 4; return isqrt(calc_atom()); }
    if (*CALC == '(') { CALC++; long v = calc_expr(); calc_ws(); if (*CALC == ')') CALC++; return v; }
    int neg = 0;
    if (*CALC == '-') { neg = 1; CALC++; }
    long n = 0;
    while (*CALC >= '0' && *CALC <= '9') { n = n * 10 + (*CALC - '0'); CALC++; }
    return neg ? -n : n;
}
static long calc_power(void) {                 /* ^ binds tighter than * / and is right-assoc */
    long v = calc_atom(); calc_ws();
    if (*CALC == '^') { CALC++; return ipow(v, calc_power()); }
    return v;
}
static long calc_term(void) {
    long v = calc_power(); calc_ws();
    while (*CALC == '*' || *CALC == '/' || *CALC == '%') {
        char op = *CALC++; long r = calc_power();
        if      (op == '*') v *= r;
        else if (op == '/') v = r ? v / r : 0;
        else                v = r ? v % r : 0;
        calc_ws();
    }
    return v;
}
static long calc_expr(void) {
    long v = calc_term(); calc_ws();
    while (*CALC == '+' || *CALC == '-') {
        char op = *CALC++; long r = calc_term();
        if (op == '+') v += r; else v -= r;
        calc_ws();
    }
    return v;
}

/* turn English math into symbols before evaluating */
static int tok_eq(const char *t, int len, const char *w) {
    int wl = (int)strlen(w); if (len != wl) return 0;
    for (int i = 0; i < len; i++) { char c = t[i]; if (c >= 'A' && c <= 'Z') c += 32; if (c != w[i]) return 0; }
    return 1;
}
static void calc_normalize(const char *in, char *out, int max) {
    int o = 0, i = 0;
    while (in[i]) {
        while (in[i] == ' ') i++;
        if (!in[i]) break;
        int s = i; while (in[i] && in[i] != ' ') i++;
        int len = i - s; const char *t = in + s; char f = t[0];
        const char *rep;
        if      (tok_eq(t,len,"times")||tok_eq(t,len,"multiplied")||tok_eq(t,len,"mult")||tok_eq(t,len,"x")) rep = "*";
        else if (tok_eq(t,len,"divided")||tok_eq(t,len,"divide")||tok_eq(t,len,"over")||tok_eq(t,len,"div")) rep = "/";
        else if (tok_eq(t,len,"plus")||tok_eq(t,len,"add")||tok_eq(t,len,"added")||tok_eq(t,len,"sum")) rep = "+";
        else if (tok_eq(t,len,"minus")||tok_eq(t,len,"subtract")||tok_eq(t,len,"less")||tok_eq(t,len,"sub")) rep = "-";
        else if (tok_eq(t,len,"mod")||tok_eq(t,len,"modulo")||tok_eq(t,len,"remainder")) rep = "%";
        else if (tok_eq(t,len,"power")||tok_eq(t,len,"pow")||tok_eq(t,len,"exponent")) rep = "^";
        else if (tok_eq(t,len,"squared")) rep = "^2";
        else if (tok_eq(t,len,"cubed")) rep = "^3";
        else if (tok_eq(t,len,"sqrt")||tok_eq(t,len,"root")) rep = "sqrt";
        else if (tok_eq(t,len,"by")||tok_eq(t,len,"the")||tok_eq(t,len,"of")||tok_eq(t,len,"to")||tok_eq(t,len,"is")||
                 tok_eq(t,len,"are")||tok_eq(t,len,"equals")||tok_eq(t,len,"equal")||tok_eq(t,len,"what")||tok_eq(t,len,"whats")||
                 tok_eq(t,len,"calculate")||tok_eq(t,len,"compute")||tok_eq(t,len,"and")||tok_eq(t,len,"a")||
                 tok_eq(t,len,"please")||tok_eq(t,len,"square")) rep = "";   /* filler -> drop */
        else if ((f>='0'&&f<='9')||f=='('||f==')'||f=='+'||f=='-'||f=='*'||f=='/'||f=='%'||f=='^'||f=='.') rep = 0; /* keep */
        else rep = "";    /* unknown word -> drop */
        if (rep == 0) { for (int k = s; k < i && o < max-2; k++) out[o++] = in[k]; out[o++] = ' '; }
        else          { for (int k = 0; rep[k] && o < max-2; k++) out[o++] = rep[k]; out[o++] = ' '; }
    }
    out[o] = 0;
}
static long calc_eval(const char *in) {
    static char norm[256];
    calc_normalize(in, norm, sizeof norm);
    CALC = norm;
    return calc_expr();
}
static void cmd_calc(const char *s) {
    if (!*s) { kputs("usage: calc <expr>   e.g.  calc 14 times 25   or   calc (2+3)*4\n"); return; }
    kprintf("= %d\n", (int)calc_eval(s));
}

/* month calendar from the real-time clock */
static int day_of_week(int y, int m, int d) {
    static int t[] = { 0,3,2,5,0,3,5,1,4,6,2,4 };
    if (m < 3) y -= 1;
    return (y + y/4 - y/100 + y/400 + t[m-1] + d) % 7;
}
static void cmd_cal(void) {
    rtc_time_t tm; rtc_now(&tm);
    int y = (int)tm.year, m = (int)tm.month; if (m < 1 || m > 12) m = 1;
    static const int md[] = { 31,28,31,30,31,30,31,31,30,31,30,31 };
    int dim = md[m-1];
    if (m == 2 && ((y%4==0 && y%100!=0) || y%400==0)) dim = 29;
    kprintf("     %s %u\n", months[m], (uint32_t)y);
    kputs(" Su Mo Tu We Th Fr Sa\n");
    int start = day_of_week(y, m, 1);
    for (int i = 0; i < start; i++) kputs("   ");
    for (int d = 1, col = start; d <= dim; d++) {
        kprintf("%s%u", d < 10 ? "  " : " ", (uint32_t)d);
        if (++col % 7 == 0) kputs("\n");
    }
    kputs("\n");
}

static void cmd_ascii(void) {
    for (int c = 32; c < 127; c++) {
        kprintf(" %u:%c", (uint32_t)c, (char)c);
        if ((c - 32) % 8 == 7) kputs("\n");
    }
    kputs("\n");
}

static uint32_t rng_state;
static void cmd_rand(void) {
    rng_state = rng_state * 1103515245u + 12345u + pit_ticks();
    kprintf("%u\n", (rng_state >> 16) % 1000);
}


/* if `dest` is a directory, turn it into dest/basename(src) */
static void into_dir(char *dest, const char *src) {
    if (!fs_isdir(dest)) return;
    const char *b = src; for (const char *q = src; *q; q++) if (*q == '/') b = q + 1;
    char j[FS_NAME_LEN]; int o = 0;
    for (const char *p = dest; *p && o < FS_NAME_LEN-1; p++) j[o++] = *p;
    if (!(o == 1 && j[0] == '/')) j[o++] = '/';
    for (const char *p = b; *p && o < FS_NAME_LEN-1; p++) j[o++] = *p;
    j[o] = 0; strcpy(dest, j);
}
static void cmd_cp(char *args) {
    char *dst = split(args);
    if (!*dst) { kputs("usage: cp <src> <dst>\n"); return; }
    char src[FS_NAME_LEN], dest[FS_NAME_LEN];
    strcpy(src, P(args)); strcpy(dest, P(dst));
    fs_file_t *s = fs_find(src);
    if (!s) { kprintf("cp: %s: not found\n", src); return; }
    if (s->is_dir) { kputs("cp: that's a directory (use mv to move it)\n"); return; }
    into_dir(dest, src);
    fs_write(dest, s->data, s->len);
    kprintf("copied %s -> %s\n", src, dest);
}
static void cmd_mv(char *args) {
    char *dst = split(args);
    if (!*dst) { kputs("usage: mv <src> <dst>\n"); return; }
    char src[FS_NAME_LEN], dest[FS_NAME_LEN];
    strcpy(src, P(args)); strcpy(dest, P(dst));
    if (!fs_find(src)) { kprintf("mv: %s: not found\n", src); return; }
    into_dir(dest, src);
    if (fs_move(src, dest) == 0) kprintf("moved %s -> %s\n", src, dest);
    else kputs("mv: failed (does the target already exist?)\n");
}
static void cmd_wc(const char *name) {
    fs_file_t *f = fs_find(name);
    if (!f) { kprintf("wc: %s: not found\n", name); return; }
    uint32_t lines = 0, words = 0, inword = 0;
    for (size_t i = 0; i < f->len; i++) {
        char ch = (char)f->data[i];
        if (ch == '\n') lines++;
        if (ch == ' ' || ch == '\n' || ch == '\t') inword = 0;
        else if (!inword) { inword = 1; words++; }
    }
    kprintf("%u lines  %u words  %u bytes  %s\n", lines, words, (uint32_t)f->len, name);
}

static void cmd_commands(void) {
    vga_setcolor(VGA_LCYAN, VGA_BLACK); kputs("All commands\n"); vga_setcolor(VGA_LGREY, VGA_BLACK);
    kputs(" help about uname commands clear cls echo history\n");
    kputs(" mem vm pgfault cpuid lspci net arp ping dns fetch\n");
    kputs(" date cal uptime rand calc ascii seq rev upper lower\n");
    kputs(" ls cat edit grep find head tail stat touch write append rm cp mv wc\n");
    kputs(" sec reboot poweroff panic   games: snake 2048 ttt\n");
    kputs(" update upgrade setup reset factory-reset shutdown restart\n");
    kputs(" bootscreen shutdownscreen restartscreen panicscreen panictest\n");
    kputs(" browse chromium gfx multitask threads ps log dmesg sync\n");
    kputs(" apps explorer files settings control draw paint clock\n");
}

/* substring test (no strstr in our libc) */
static int contains(const char *h, const char *n) {
    if (!*n) return 1;
    for (; *h; h++) {
        const char *a = h, *b = n;
        while (*a && *b && *a == *b) { a++; b++; }
        if (!*b) return 1;
    }
    return 0;
}

static void cmd_grep(char *args) {
    char *file = split(args);                /* args=pattern, file=filename */
    if (!*file) { kputs("usage: grep <pattern> <file>\n"); return; }
    fs_file_t *f = fs_find(P(file));
    if (!f) { kprintf("grep: %s: not found\n", file); return; }
    char line[160]; size_t li = 0; int hits = 0;
    for (size_t i = 0; i <= f->len; i++) {
        char ch = (i < f->len) ? (char)f->data[i] : '\n';
        if (ch == '\n') { line[li] = 0; if (li && contains(line, args)) { kprintf("  %s\n", line); hits++; } li = 0; }
        else if (li < 159) line[li++] = ch;
    }
    if (!hits) kputs("  (no matches)\n");
}

static void cmd_find(const char *pat) {
    int shown = 0;
    for (int i = 0; i < FS_MAX_FILES; i++) {
        fs_file_t *f = fs_at(i);
        if (f && f->used && contains(f->name, pat)) { kprintf("  %s\n", f->name); shown++; }
    }
    if (!shown) kputs("  (no matches)\n");
}

static void cmd_head(const char *name) {
    fs_file_t *f = fs_find(name);
    if (!f) { kprintf("head: %s: not found\n", name); return; }
    int lines = 0;
    for (size_t i = 0; i < f->len && lines < 10; i++) {
        char ch = (char)f->data[i]; char s[2] = { ch, 0 }; kputs(s);
        if (ch == '\n') lines++;
    }
    kputs("\n");
}

static void cmd_tail(const char *name) {
    fs_file_t *f = fs_find(name);
    if (!f) { kprintf("tail: %s: not found\n", name); return; }
    int total = 0;
    for (size_t i = 0; i < f->len; i++) if (f->data[i] == '\n') total++;
    int skip = total > 10 ? total - 10 : 0, seen = 0;
    size_t i = 0;
    for (; i < f->len && seen < skip; i++) if (f->data[i] == '\n') seen++;
    for (; i < f->len; i++) { char s[2] = { (char)f->data[i], 0 }; kputs(s); }
    kputs("\n");
}

static void cmd_rev(const char *s) {
    size_t n = strlen(s);
    for (size_t i = n; i > 0; i--) { char b[2] = { s[i-1], 0 }; kputs(b); }
    kputs("\n");
}

static void cmd_case(const char *s, int up) {
    for (; *s; s++) {
        char c = *s;
        if (up && c >= 'a' && c <= 'z') c -= 32;
        else if (!up && c >= 'A' && c <= 'Z') c += 32;
        char b[2] = { c, 0 }; kputs(b);
    }
    kputs("\n");
}

static void cmd_seq(const char *s) {
    int n = 0;
    while (*s >= '0' && *s <= '9') { n = n * 10 + (*s - '0'); s++; }
    if (n > 500) n = 500;
    for (int i = 1; i <= n; i++) kprintf("%u ", (uint32_t)i);
    kputs("\n");
}

/* ---------- more Linux-style coreutils ---------- */
static void cmd_pwd(void)      { kprintf("%s\n", cwd); }
/* the owner of a single-user machine is the admin (unless role says otherwise) */
static int is_admin(void) { char r[16]; if (!cfg_get("role", r, sizeof r) || !r[0]) return 1; return !strcmp(r, "admin"); }
static void cmd_whoami(void)   { char u[40]; if (!cfg_get("user", u, sizeof u) || !u[0]) strcpy(u, "user"); kputs(u); kputs(is_admin() ? " (admin)\n" : "\n"); }
static void cmd_hostname(void) { char h[40]; cfg_hostname(h, sizeof h); kputs(h); kputs("\n"); }
static void cmd_env(void) {
    char h[40], u[40]; cfg_hostname(h, sizeof h);
    if (!cfg_get("user", u, sizeof u) || !u[0]) strcpy(u, "user");
    kprintf("USER=%s\nHOST=%s\nOS=HisokaOS 0.2\nSHELL=hsh\nTERM=vga-80x25\nHOME=/home\n", u, h);
}
static void cmd_yes(const char *s) { const char *t = *s ? s : "y"; for (int i = 0; i < 18; i++) { kputs(t); kputs("\n"); } }
static void cmd_factor(const char *s) {
    long n = 0; while (*s >= '0' && *s <= '9') { n = n*10 + (*s-'0'); s++; }
    if (n < 2) { kprintf("%d: -\n", (int)n); return; }
    kprintf("%d:", (int)n); long m = n;
    for (long d = 2; d*d <= m; d++) while (m % d == 0) { kprintf(" %d", (int)d); m /= d; }
    if (m > 1) kprintf(" %d", (int)m);
    kputs("\n");
}
static void cmd_basename(const char *s) {
    const char *b = s; for (const char *p = s; *p; p++) if (*p == '/') b = p + 1;
    kputs(*b ? b : "/"); kputs("\n");
}
static void cmd_dirname(const char *s) {
    const char *last = 0; for (const char *p = s; *p; p++) if (*p == '/') last = p;
    if (!last) { kputs(".\n"); return; }
    for (const char *p = s; p < last; p++) { char c[2] = { *p, 0 }; kputs(c); }
    kputs("\n");
}
static void cmd_tac(const char *name) {
    fs_file_t *f = fs_find(name);
    if (!f) { kprintf("tac: %s: not found\n", name); return; }
    int starts[256], ns = 0; starts[ns++] = 0;
    for (size_t i = 0; i < f->len; i++) if (f->data[i] == '\n' && ns < 256) starts[ns++] = (int)i + 1;
    for (int li = ns - 1; li >= 0; li--) {
        int a = starts[li], e = (li + 1 < ns) ? starts[li+1] - 1 : (int)f->len;
        for (int i = a; i < e; i++) { char c[2] = { (char)f->data[i], 0 }; kputs(c); }
        kputs("\n");
    }
}
static void cmd_nl(const char *name) {
    fs_file_t *f = fs_find(name);
    if (!f) { kprintf("nl: %s: not found\n", name); return; }
    uint32_t ln = 1; kprintf("%u  ", ln);
    for (size_t i = 0; i < f->len; i++) {
        char c = (char)f->data[i]; char s[2] = { c, 0 }; kputs(s);
        if (c == '\n') kprintf("%u  ", ++ln);
    }
    kputs("\n");
}
static void cmd_sort(const char *name) {
    fs_file_t *f = fs_find(name);
    if (!f) { kprintf("sort: %s: not found\n", name); return; }
    static char L[128][128]; int n = 0, li = 0;
    for (size_t i = 0; i < f->len && n < 128; i++) {
        char c = (char)f->data[i];
        if (c == '\n') { L[n][li] = 0; n++; li = 0; }
        else if (li < 127) L[n][li++] = c;
    }
    if (li && n < 128) { L[n][li] = 0; n++; }
    for (int a = 1; a < n; a++) { char tmp[128]; strcpy(tmp, L[a]); int b = a-1;
        while (b >= 0 && strcmp(L[b], tmp) > 0) { strcpy(L[b+1], L[b]); b--; } strcpy(L[b+1], tmp); }
    for (int a = 0; a < n; a++) { kputs(L[a]); kputs("\n"); }
}
static void cmd_uniq(const char *name) {
    fs_file_t *f = fs_find(name);
    if (!f) { kprintf("uniq: %s: not found\n", name); return; }
    static char prev[160], cur[160]; int li = 0, first = 1;
    for (size_t i = 0; i <= f->len; i++) {
        char c = (i < f->len) ? (char)f->data[i] : '\n';
        if (c == '\n') { cur[li] = 0; if (first || strcmp(cur, prev)) { kputs(cur); kputs("\n"); strcpy(prev, cur); first = 0; } li = 0; }
        else if (li < 159) cur[li++] = c;
    }
}
static void cmd_hexdump(const char *name) {
    fs_file_t *f = fs_find(name);
    if (!f) { kprintf("hexdump: %s: not found\n", name); return; }
    const char *H = "0123456789abcdef";
    for (size_t i = 0; i < f->len; i += 16) {
        kprintf("%x: ", (uint32_t)i);
        for (size_t j = 0; j < 16; j++) {
            if (i + j < f->len) { uint8_t b = f->data[i+j]; char s[4] = { H[b>>4], H[b&15], ' ', 0 }; kputs(s); }
            else kputs("   ");
        }
        kputs(" ");
        for (size_t j = 0; j < 16 && i + j < f->len; j++) {
            uint8_t b = f->data[i+j]; char c = (b >= 32 && b < 127) ? (char)b : '.'; char s[2] = { c, 0 }; kputs(s);
        }
        kputs("\n");
    }
}
static void cmd_df(void) { kprintf("ramfs   %u/%u files used\n", (uint32_t)fs_count(), FS_MAX_FILES); }
static void cmd_du(void) {
    uint32_t tot = 0;
    for (int i = 0; i < FS_MAX_FILES; i++) { fs_file_t *f = fs_at(i);
        if (f && f->used) { kprintf("  %u  %s\n", (uint32_t)f->len, f->name); tot += f->len; } }
    kprintf("  total %u bytes\n", tot);
}
static void cmd_sleep(const char *s) {
    int n = 0; while (*s >= '0' && *s <= '9') { n = n*10 + (*s-'0'); s++; }
    if (n > 30) n = 30;
    uint32_t t0 = pit_ticks();
    while (pit_ticks() - t0 < (uint32_t)n * 100) __asm__ volatile("hlt");
}

/* ---------- our own commands / shortcuts ---------- */
static uint32_t rng2;
static uint32_t rnd2(void) { rng2 = rng2*1103515245u + 12345u + pit_ticks(); return rng2 >> 16; }
static void cmd_flip(void) { kputs((rnd2() & 1) ? "heads\n" : "tails\n"); }
static void cmd_roll(const char *s) {
    int n = 0; while (*s >= '0' && *s <= '9') { n = n*10 + (*s-'0'); s++; }
    if (n < 2) n = 6;
    kprintf("%u\n", rnd2() % (uint32_t)n + 1);
}
static void cmd_len(const char *s) { kprintf("%u\n", (uint32_t)strlen(s)); }
static void cmd_hex(const char *s) { int n = 0; while (*s>='0'&&*s<='9'){n=n*10+(*s-'0');s++;} kprintf("0x%x\n", (uint32_t)n); }
static void cmd_bin(const char *s) {
    uint32_t n = 0; while (*s>='0'&&*s<='9'){n=n*10+(*s-'0');s++;}
    char b[33]; int i = 0; int started = 0;
    for (int bit = 31; bit >= 0; bit--) { int v = (n >> bit) & 1; if (v) started = 1; if (started) b[i++] = (char)('0'+v); }
    if (!i) b[i++] = '0'; b[i] = 0; kprintf("%s\n", b);
}
static void cmd_ord(const char *s) { kprintf("%u\n", (uint32_t)(uint8_t)s[0]); }
static void cmd_chr(const char *s) { int n=0; while(*s>='0'&&*s<='9'){n=n*10+(*s-'0');s++;} char c[2]={(char)n,0}; kputs(c); kputs("\n"); }
static void cmd_box(const char *s) {
    int len = (int)strlen(s);
    kputs("+"); for (int i = 0; i < len + 2; i++) kputs("-"); kputs("+\n");
    kprintf("| %s |\n", s);
    kputs("+"); for (int i = 0; i < len + 2; i++) kputs("-"); kputs("+\n");
}
static void cmd_cowsay(const char *s) {
    if (!*s) s = "moo";
    int len = (int)strlen(s);
    kputs(" "); for (int i = 0; i < len + 2; i++) kputs("_"); kputs("\n");
    kprintf("< %s >\n", s);
    kputs(" "); for (int i = 0; i < len + 2; i++) kputs("-"); kputs("\n");
    kputs("        \\   ^__^\n");
    kputs("         \\  (oo)\\_______\n");
    kputs("            (__)\\       )\\/\\\n");
    kputs("                ||----w |\n");
    kputs("                ||     ||\n");
}
static void cmd_repeat(char *args) {
    char *txt = split(args);
    int n = 0; for (char *p = args; *p >= '0' && *p <= '9'; p++) n = n*10 + (*p-'0');
    if (n > 50) n = 50;
    for (int i = 0; i < n; i++) { kputs(txt); kputs("\n"); }
}
static void cmd_motd(void) {
    vga_setcolor(VGA_LCYAN, VGA_BLACK);
    kputs("  Welcome to HisokaOS - a from-scratch x86 operating system.\n");
    vga_setcolor(VGA_LGREY, VGA_BLACK);
    kputs("  'help' for groups, 'commands' for everything, 'man' for the manual.\n");
}
static void cmd_apps(void) {
    vga_setcolor(VGA_LCYAN, VGA_BLACK); kputs("Apps  (type the name to launch)\n");
    vga_setcolor(VGA_LGREY, VGA_BLACK);
    kputs("  edit <file>  full-screen text editor      explorer   browse files\n");
    kputs("  settings     Control Center (theme+info)  draw       ASCII paint canvas\n");
    kputs("  clock        big-digit RTC clock          snake      the classic\n");
    kputs("  2048         sliding-tile puzzle          ttt        tic-tac-toe vs CPU\n");
}
/* three kernel threads that interleave to demonstrate the scheduler */
static void mt_a(void) { for (int i = 0; i < 6; i++) { kprintf(" A%d", i); task_yield(); } }
static void mt_b(void) { for (int i = 0; i < 6; i++) { kprintf(" B%d", i); task_yield(); } }
static void mt_c(void) { for (int i = 0; i < 6; i++) { kprintf(" C%d", i); task_yield(); } }
static void cmd_multitask(void) {
    vga_setcolor(VGA_LCYAN, VGA_BLACK); kputs("Multitasking\n"); vga_setcolor(VGA_LGREY, VGA_BLACK);
    kputs("  spawning 3 kernel threads; the round-robin scheduler interleaves them:\n ");
    task_create(mt_a); task_create(mt_b); task_create(mt_c);
    for (int i = 0; i < 60 && task_count() > 1; i++) task_yield();
    kputs("\n  all threads finished - real context switching between tasks.\n");
}
static void cmd_ps(void) {
    kprintf("  %d task(s) running; current = task #%d\n", task_count(), task_current());
    kputs("  (task 0 is the shell; 'multitask' spawns more)\n");
}
static void cmd_keys(void) {
    vga_setcolor(VGA_LCYAN, VGA_BLACK); kputs("Keyboard shortcuts (shell line editing)\n");
    vga_setcolor(VGA_LGREY, VGA_BLACK);
    kputs("  Left/Ctrl-B back    Right/Ctrl-F forward    Ctrl-A home    Ctrl-E end\n");
    kputs("  Backspace del-left  Ctrl-D delete  Ctrl-U kill-line  Ctrl-K kill-to-end\n");
    kputs("  Ctrl-W del-word     Ctrl-L clear   Ctrl-C cancel     Tab complete\n");
    kputs("  Up/Ctrl-P prev cmd  Down/Ctrl-N next cmd   editor: Ctrl-S save, Ctrl-Q quit\n");
}

static void dispatch(char *line);   /* forward declaration for the script runner */

/* identify a file's type from its extension */
static const char *ext_of(const char *name) {
    const char *dot = 0; for (const char *p = name; *p; p++) if (*p == '.') dot = p;
    return dot ? dot + 1 : "";
}
static void cmd_file(const char *name) {
    fs_file_t *f = fs_find(name);
    if (!f) { kprintf("file: %s: not found\n", name); return; }
    const char *e = ext_of(name), *d;
    if      (!strcmp(e, "txt"))  d = "plain text";
    else if (!strcmp(e, "md"))   d = "Markdown document";
    else if (!strcmp(e, "sh"))   d = "shell script (run with 'run')";
    else if (!strcmp(e, "cfg") || !strcmp(e, "conf") || !strcmp(e, "ini")) d = "configuration";
    else if (!strcmp(e, "log"))  d = "log file";
    else if (!strcmp(e, "c") || !strcmp(e, "h")) d = "C source";
    else if (!strcmp(e, "json")) d = "JSON data";
    else if (!strcmp(e, "csv"))  d = "CSV data";
    else if (!strcmp(e, "note")) d = "note";
    else if (!*e)                d = "data (no extension)";
    else                         d = "data";
    int text = 1;
    for (size_t i = 0; i < f->len && i < 256; i++) {
        uint8_t b = f->data[i];
        if (b != '\n' && b != '\t' && b != '\r' && (b < 32 || b > 126)) { text = 0; break; }
    }
    kprintf("%s: %s, %u bytes, %s\n", name, d, (uint32_t)f->len, text ? "ASCII text" : "binary");
}

/* run a .sh file: execute each non-comment line as a shell command */
static void cmd_run(const char *name) {
    fs_file_t *f = fs_find(name);
    if (!f) { kprintf("run: %s: not found\n", name); return; }
    char line[LINE]; int li = 0;
    for (size_t i = 0; i <= f->len; i++) {
        char c = (i < f->len) ? (char)f->data[i] : '\n';
        if (c == '\n') {
            line[li] = 0;
            char *a = line; while (*a == ' ') a++;
            if (*a && *a != '#') {
                vga_setcolor(VGA_DGREY, VGA_BLACK); kprintf("$ %s\n", a); vga_setcolor(VGA_LGREY, VGA_BLACK);
                char tmp[LINE]; strcpy(tmp, a); dispatch(tmp);
            }
            li = 0;
        } else if (li < LINE - 1) line[li++] = c;
    }
}

/* seed the filesystem with starter files of several types (only if absent) */
static void seed(const char *name, const char *content) {
    if (!fs_find(name)) fs_write(name, content, strlen(content));
}
static void seed_files(void) {
    /* standard directory tree (like /home, /etc, /var on a real system) */
    fs_mkdir("/home");  fs_mkdir("/home/Documents"); fs_mkdir("/home/Downloads");
    fs_mkdir("/home/Pictures"); fs_mkdir("/home/Music"); fs_mkdir("/home/Desktop");
    fs_mkdir("/etc");   fs_mkdir("/bin");  fs_mkdir("/var"); fs_mkdir("/var/log");
    fs_mkdir("/System"); fs_mkdir("/System/Library"); fs_mkdir("/System/Drivers");
    fs_mkdir("/System/Fonts"); fs_mkdir("/tmp"); fs_mkdir("/root");

    /* system files under /etc */
    seed("/etc/hostname",   "hisoka\n");
    seed("/etc/version",    "HisokaOS 0.2\n");
    seed("/etc/system.cfg", "# HisokaOS configuration\nhostname=hisoka\nuser=user\ntheme=blue\nshell=hsh\n");
    seed("/etc/motd",       "Welcome to HisokaOS.\n");
    seed("/etc/hosts",      "127.0.0.1 localhost\n10.0.2.15 hisoka\n");
    seed("/etc/passwd",     "user:x:1000:1000:HisokaOS user:/home:/bin/hsh\n");
    /* the /System folder - core OS files, like a real OS */
    seed("/System/about.txt",   "HisokaOS - a from-scratch 32-bit x86 operating system.\nKernel, drivers, filesystem, networking and apps written from scratch.\n");
    seed("/System/version.txt", "HisokaOS 0.2 (i386)\nbuild: from-scratch C kernel\n");
    seed("/System/kernel.txt",  "monolithic kernel: GDT, IDT, paging, scheduler, ramfs,\nATA disk, RTL8139 network, VGA + VBE graphics.\n");
    seed("/System/Drivers/list.txt", "vga (text+graphics)  keyboard  pit  rtc  serial\nata (disk)  rtl8139 (network)  pci\n");
    seed("/System/license.txt", "HisokaOS is a personal from-scratch project by Jeffery / Humane AI.\n");

    /* the user's home */
    seed("/home/welcome.txt",
        "Welcome to HisokaOS!\n"
        "This is your home directory. Use 'ls', 'cd', 'mkdir' to get around.\n"
        "Your folders: Documents, Downloads, Pictures, Music, Desktop.\n"
        "Type 'man' for the manual, 'menu' for the Home screen.\n");
    seed("/home/readme.md",
        "# HisokaOS 0.2\n"
        "A from-scratch 32-bit x86 operating system.\n\n"
        "- Real TCP/IP networking, a text browser, persistent disk\n"
        "- Directory tree (/home, /etc, /var/log), a system log\n"
        "- Text editor, file explorer, apps and games\n");
    seed("/home/Documents/todo.txt",
        "[ ] explore the file tree with 'ls' and 'cd'\n"
        "[ ] write a script and 'run' it\n"
        "[ ] fetch a website\n");
    seed("/home/hello.sh",
        "# a HisokaOS shell script - run it with:  run hello.sh\n"
        "motd\n"
        "uname\n"
        "mem\n"
        "cowsay scripts work on HisokaOS\n");
    seed("/home/Documents/notes.md",
        "# Notes\n"
        "- Everything here persists: run 'sync' to save to disk.\n");

    /* ---- a full, real system tree: everything in the OS, browsable as files ---- */
    fs_mkdir("/dev"); fs_mkdir("/proc"); fs_mkdir("/usr"); fs_mkdir("/usr/share");
    fs_mkdir("/usr/share/doc"); fs_mkdir("/usr/share/doc/hisokaos");
    fs_mkdir("/System/Kernel"); fs_mkdir("/System/Network");

    /* /etc - configuration, like a real Unix */
    seed("/etc/os-release", "NAME=HisokaOS\nVERSION=0.2\nID=hisokaos\nARCH=i386\nPRETTY_NAME=\"HisokaOS 0.2 (i386)\"\nHOME_URL=local\n");
    seed("/etc/group",      "root:x:0:\nusers:x:4:user\nwheel:x:10:user\n");
    seed("/etc/fstab",      "# device   mountpoint  type   options\nramfs      /           ramfs  rw\ndisk0      /           ata    sync   # 'sync' command persists\n");
    seed("/etc/shells",     "/bin/hsh\n");
    seed("/etc/profile",    "# login shell profile\nexport PS1=hisoka\nexport PATH=/bin\numask 022\n");
    seed("/etc/resolv.conf","nameserver 10.0.2.3\n");
    seed("/etc/services",   "echo    7/udp\nhttp    80/tcp\ndns     53/udp\nhttps   443/tcp\n");
    seed("/etc/network.cfg","# network (QEMU SLIRP user mode)\ndhcp=on\nip=10.0.2.15\ngateway=10.0.2.2\ndns=10.0.2.3\n");
    seed("/etc/timezone",   "UTC\n");

    /* /dev - device nodes (described) */
    seed("/dev/console", "the system console (VGA text mode, 80x25)\n");
    seed("/dev/null",    "the bit bucket - writes vanish, reads return nothing\n");
    seed("/dev/zero",    "an endless stream of zero bytes\n");
    seed("/dev/tty",     "the controlling terminal\n");
    seed("/dev/kbd",     "PS/2 keyboard, IRQ1, ring-buffered\n");
    seed("/dev/vga",     "VGA: text mode + a Bochs/VBE linear framebuffer (graphics)\n");
    seed("/dev/disk0",   "ATA PIO disk; the ramfs persists to a blob at sector 0\n");
    seed("/dev/net0",    "RTL8139 NIC (PCI 10ec:8139), 10.0.2.15\n");
    seed("/dev/serial",  "COM1 (0x3F8) - the kernel mirrors all output here\n");
    seed("/dev/random",  "a small PRNG seeded from the timer\n");

    /* /System/Drivers - one file per driver */
    seed("/System/Drivers/vga.txt",      "VGA text 80x25 (0xB8000) + VBE linear framebuffer 1024x768x32.\nThemeable chrome bars and side borders.\n");
    seed("/System/Drivers/keyboard.txt", "PS/2 keyboard on IRQ1. Scancode set 1 -> ASCII, arrows/Home/End/PgUp/PgDn/Del.\n");
    seed("/System/Drivers/pit.txt",      "8253/8254 PIT at 100 Hz drives the uptime tick and app timing.\n");
    seed("/System/Drivers/rtc.txt",      "CMOS RTC provides the wall clock (date/time).\n");
    seed("/System/Drivers/serial.txt",   "16550 UART on COM1 (0x3F8) for headless logging.\n");
    seed("/System/Drivers/ata.txt",      "ATA PIO driver: read/write sectors; backs the persistent filesystem.\n");
    seed("/System/Drivers/rtl8139.txt",  "Realtek 8139 NIC: TX/RX DMA rings. Foundation of the network stack.\n");
    seed("/System/Drivers/pci.txt",      "PCI bus enumeration via ports 0xCF8/0xCFC ('lspci').\n");

    /* /System/Kernel - the OS documents its own internals (open source) */
    seed("/System/Kernel/gdt.txt",      "Global Descriptor Table: flat code/data segments + ring0/ring3.\n");
    seed("/System/Kernel/idt.txt",      "Interrupt Descriptor Table: 32 CPU exceptions + 16 IRQs.\n");
    seed("/System/Kernel/paging.txt",   "Paging: 4 MiB PSE pages, identity-mapped low memory, #PF handler.\n");
    seed("/System/Kernel/scheduler.txt","Cooperative round-robin scheduler with an assembly context switch.\n");
    seed("/System/Kernel/memory.txt",   "PMM frame allocator + a kmalloc/kfree heap above it.\n");
    seed("/System/Kernel/ramfs.txt",    "Hierarchical in-RAM filesystem; full-path nodes; persists to disk.\n");
    seed("/System/build.txt",           "Built with clang -target i686-elf + i686-elf-ld. Multiboot1 ELF.\n");

    /* /var/log - logs (system.log is written live by klog) */
    seed("/var/log/boot.log",   "boot: multiboot ok -> gdt idt isr pic pit kbd -> pmm heap paging -> ramfs disk pci net -> shell\n");
    seed("/var/log/kernel.log", "kernel: subsystems initialized; see 'dmesg' for the live boot buffer\n");
    seed("/var/log/net.log",    "net: RTL8139 up, DHCP/SLIRP address 10.0.2.15\n");
    seed("/var/log/auth.log",   "auth: console login as 'user'\n");

    /* /usr/share/doc - open-source documentation */
    seed("/usr/share/doc/hisokaos/README.txt",
        "HisokaOS - a from-scratch 32-bit x86 operating system.\n"
        "Open source, built by hand: kernel, drivers, TCP/IP, filesystem and apps.\n"
        "Browse /System, /proc, /dev and /etc to see how it all works.\n");
    seed("/usr/share/doc/hisokaos/architecture.txt",
        "boot.s -> kernel_main: bring up GDT, IDT, PIC, PIT, keyboard, PMM, heap,\n"
        "paging, the ramfs, the ATA disk, PCI and the RTL8139, then the shell.\n");
    seed("/usr/share/doc/hisokaos/networking.txt",
        "A from-scratch stack: Ethernet -> ARP -> IPv4 -> ICMP/UDP/TCP -> DNS/HTTP.\n"
        "'net', 'ping', 'dns', 'fetch', 'browse' and 'download' use it.\n");
    seed("/usr/share/doc/hisokaos/filesystem.txt",
        "ramfs holds files in kmalloc'd buffers with full-path names. 'sync' writes a\n"
        "blob to ATA sector 0; boot restores it. /proc is generated live on read.\n");

    /* /sys - CONTROL files: write to these and the OS reacts live (like Linux /sys) */
    fs_mkdir("/sys");
    { char h[40]; cfg_hostname(h, sizeof h); char hn[42]; int n=0; for(const char*p=h;*p;p++)hn[n++]=*p; hn[n]=0; seed("/sys/hostname", hn); }
    { char t[16]; if (!cfg_get("theme", t, sizeof t)) strcpy(t, "blue"); seed("/sys/theme", t); }
    { char r[16]; if (!cfg_get("role", r, sizeof r) || !r[0]) strcpy(r, "admin"); seed("/sys/role", r); }
    seed("/sys/title", "HisokaOS 0.2\n");
    seed("/sys/README.txt",
        "/sys - control the OS by WRITING to these files (like Linux /sys).\n"
        "  /sys/theme     write a color (blue green cyan red magenta grey amber)\n"
        "                 -> the accent recolors live, and it persists.\n"
        "  /sys/hostname  write a name -> the shell hostname changes.\n"
        "  /sys/title     write text  -> the top title bar changes.\n"
        "Try:  write /sys/theme green     or edit it in Build and save.\n"
        "Reading them (cat) shows the current value.\n");

    procfs_refresh();   /* generate the live /proc files */
    agentinfo_seed();   /* /System/AGENTS.md + /etc/llms.txt: what HisokaOS is, for AI agents */
}

/* ---------- the manual (also written to manual.txt in the file explorer) ---------- */
static const char MANUAL[] =
"HisokaOS Manual\n"
"===============\n"
"(Space/b to page, Up/Dn to scroll, q to quit)\n"
"\n"
"GETTING AROUND\n"
"  Home screen: arrow keys or the MOUSE - hover to highlight,\n"
"  click to open. Esc drops to the terminal.\n"
"  help        all commands, grouped and scrollable\n"
"  commands    the full flat list   man   this manual\n"
"  menu        back to the Home screen   clear   clear the screen\n"
"\n"
"FILES AND FOLDERS  (everything is a file)\n"
"  ls [dir]   cd <dir>   pwd   tree   cat <f>   more <f>\n"
"  build <f>  open a file in the Build editor\n"
"  touch <f>  write <f> <text>   append <f> <text>\n"
"  mkdir <d>  rmdir <d>   rm <f>   cp a b   mv a b   stat <f>\n"
"  grep <word> <f>   find <word>   wc <f>   open <file|app>\n"
"  sync       save everything to disk\n"
"  The whole OS is browsable: /etc /dev /proc /sys /System /var/log.\n"
"  /proc is live (cat /proc/meminfo). Write /sys to control the OS:\n"
"  e.g.  write /sys/theme green   changes the accent color live.\n"
"\n"
"FILES APP\n"
"  Open 'Files' or type 'explorer'. Arrows/mouse to move, Enter or\n"
"  click to open, right-click for a menu (rename, delete, copy...).\n"
"  Keys: n new file  m new folder  r rename  d delete  c copy\n"
"        x cut  v paste  t send-to-terminal  q quit\n"
"\n"
"BUILD - the code editor\n"
"  Left sidebar lists your files; pick one or press N for new.\n"
"  Line numbers + syntax highlighting. Ctrl-S save, Esc back to\n"
"  files, Ctrl-W find, Ctrl-G go-to-line.\n"
"\n"
"APPS (60+, all in /Applications - open from Files or by name)\n"
"  Alexis      your assistant (ask, code) - streamed from the host\n"
"  Media       color-pixel demo, image viewer, slideshow\n"
"  Tools menu  todo, notes, calendar, contacts, monitor, finance,\n"
"              units, calculators, converters, piano and many more\n"
"  Draw  Clock  Settings  Games (snake 2048 tetris ttt)\n"
"\n"
"THE WEB\n"
"  fetch <site>      text browser (http only)\n"
"  browse <site>     streamed Chromium page (needs the host helper)\n"
"  download <url> <dest>   save a web file into a folder\n"
"  dns <name>   ping <host>   net   network status\n"
"\n"
"SYSTEM\n"
"  setup    re-run the setup wizard      update   check for updates\n"
"  reset confirm   factory reset         changelog   what's new\n"
"  mem  vm  uname  uptime  lspci  cpuid   sec   protections\n"
"  log / dmesg   the system log (everything is logged)\n"
"  shutdown   restart   beep   piano (the PC speaker)\n"
"\n"
"SHELL SHORTCUTS\n"
"  Left/Ctrl-B back   Right/Ctrl-F forward   Ctrl-A/E start/end\n"
"  Ctrl-U clear line  Ctrl-K kill-to-end   Ctrl-W delete word\n"
"  Ctrl-L clear   Tab complete   Up/Down history\n"
"\n"
"FILE TYPES\n"
"  .txt text   .md markdown   .sh script (run it)   .c/.h source\n"
"  .cfg/.conf config   .json/.csv data   .bmp image\n"
"\n"
"NOTE: this is a from-scratch kernel - no Linux layer, so external\n"
"programs (Python, Homebrew) can't run yet. Every command is built\n"
"in. Run 'help' any time to see them all.\n";

static void write_manual(void) { fs_write("/home/manual.txt", MANUAL, sizeof(MANUAL) - 1); }
static void cmd_man(void)      { write_manual(); pager(MANUAL); }

/* page through any file's contents */
static void cmd_more(const char *path) {
    fs_file_t *f = fs_find(path);
    if (!f)         { kprintf("more: %s: not found\n", path); return; }
    if (f->is_dir)  { kprintf("more: %s is a directory\n", path); return; }
    static char tmp[16384];
    size_t n = f->len < sizeof(tmp) - 1 ? f->len : sizeof(tmp) - 1;
    memcpy(tmp, f->data, n); tmp[n] = 0;
    pager(tmp);
}

/* ---------- Home: a friendly arrow-key launcher ---------- */
static void menu_input(const char *prompt, char *buf, int max) {
    vga_setcolor(VGA_WHITE, VGA_BLACK); kputs(prompt);
    int len = 0; buf[0] = 0;
    int sx, sy; vga_getcursor(&sx, &sy);
    for (;;) {
        char c = keyboard_getc();
        if (c == '\n' || c == '\r') { buf[len] = 0; kputs("\n"); return; }
        else if (c == 27) { buf[0] = 0; kputs("\n"); return; }
        else if (c == '\b') { if (len) { len--; vga_cell(sx+len, sy, ' ', VGA_WHITE, VGA_BLACK); vga_setcursor(sx+len, sy); } }
        else if (c >= 32 && c < 127 && len < max-1) { buf[len++] = c; vga_cell(sx+len-1, sy, c, VGA_WHITE, VGA_BLACK); vga_setcursor(sx+len, sy); }
    }
}

static int menu_pick(const char *title, const char *items[], int n) {
    int sel = 0, top = 0, pbtn = 0;
    const int VIS = 19;                 /* visible rows: y = 4 .. 22 */
    for (;;) {
        if (sel < top) top = sel;
        if (sel >= top + VIS) top = sel - VIS + 1;
        vga_clear();
        vga_setcolor(VGA_YELLOW, VGA_BLACK); kprintf(" %s\n\n", title);
        vga_setcolor(VGA_LGREY, VGA_BLACK);
        for (int r = 0; r < VIS; r++) {
            int i = top + r; if (i >= n) break;
            int y = 4 + r; uint8_t fg = (i==sel)?VGA_BLACK:VGA_LGREY, bg = (i==sel)?VGA_LGREY:VGA_BLACK;
            for (int x = 2; x <= 50; x++) vga_cell(x, y, ' ', fg, bg);
            vga_cell(3, y, (i==sel)?'>':' ', fg, bg);
            for (int k = 0; items[i][k] && k < 44; k++) vga_cell(5+k, y, items[i][k], fg, bg);
        }
        if (n > VIS) { char ind[16]; int o=0; for(const char*p=" ";*p;p++)ind[o++]=*p; ind[o++]=(char)('0'+(sel+1)/10%10); ind[o++]=(char)('0'+(sel+1)%10); ind[o++]='/'; ind[o++]=(char)('0'+n/10%10); ind[o++]=(char)('0'+n%10); ind[o]=0; for(int i=0;ind[i];i++) vga_cell(54+i,4,ind[i],VGA_DGREY,VGA_BLACK); }
        vga_setcursor(0, 24);
        for (;;) {
            char c = keyboard_trygetc();
            if (c == 'q' || c == 27) return -1;
            else if (c == 0x10) { sel = (sel + n - 1) % n; break; }
            else if (c == 0x0E) { sel = (sel + 1) % n; break; }
            else if (c == '\n') return sel;
            if (mouse_present()) {
                int mx, my, b; mouse_get(&mx, &my, &b);
                int r = my - 4, idx = (r >= 0 && r < VIS && mx >= 2 && mx <= 50) ? top + r : -1;
                if (idx >= n) idx = -1;
                if ((b & 1) && !(pbtn & 1)) { pbtn = b; if (idx >= 0) return idx; break; }
                pbtn = b;
                if (idx >= 0 && idx != sel) { sel = idx; break; }
            }
            __asm__ volatile("hlt");
        }
    }
}

/* open a URL and then let the user follow its links by number, or type a new site */
static void open_url(const char *url) {
    char target[160]; strcpy(target, url);
    for (;;) {
        vga_clear(); cmd_fetch(target);
        char in[80];
        vga_setcolor(VGA_DGREY, VGA_BLACK);
        menu_input(g_nlinks ? "\n  Follow link # (or type a site, blank = back): "
                            : "\n  Type a site (blank = back): ", in, 80);
        vga_setcolor(VGA_LGREY, VGA_BLACK);
        if (!in[0]) return;
        int num = 0, isnum = 1;
        for (int i = 0; in[i]; i++) { if (in[i] < '0' || in[i] > '9') { isnum = 0; break; } num = num*10 + (in[i]-'0'); }
        if (isnum && num >= 1 && num <= g_nlinks) {
            const char *l = g_links[num-1];
            if (starts_with_ci(l, "http")) strcpy(target, l);
            else { int o = 0; for (const char *p = g_host; *p && o < 159; p++) target[o++] = *p;
                   if (l[0] != '/' && o < 159) target[o++] = '/';
                   for (const char *p = l; *p && o < 159; p++) target[o++] = *p; target[o] = 0; }
        } else strcpy(target, in);
    }
}
static void menu_web(void) {
    static const char *items[] = {
        "Chromium  - open a site (real graphical page)",
        "Chromium  - example.com",
        "Text browser - fast, follow links by number",
    };
    for (;;) {
        vga_statusbar(" WEB BROWSER   Up/Down select   Enter open   q back");
        int s = menu_pick("Web Browser", items, 3);
        if (s < 0) return;
        if (s == 0) {
            char host[80]; vga_clear();
            menu_input("  Open site in Chromium (e.g. example.com): ", host, 80);
            if (host[0]) browser_run(host);
        } else if (s == 1) {
            browser_run("example.com");
        } else {
            static const char *marks[] = { "example.com", "info.cern.ch", "Type a site..." };
            vga_statusbar(" TEXT BROWSER   Up/Down select   Enter open   q back");
            int t = menu_pick("Text Browser", marks, 3);
            if (t < 0) continue;
            char host[64];
            if (t == 2) { vga_clear(); menu_input("  Enter site (e.g. example.com): ", host, 64); if (!host[0]) continue; }
            else strcpy(host, marks[t]);
            open_url(host);
        }
    }
}

static void menu_games(void) {
    static const char *g[] = { "Snake", "2048", "Tetris", "Tic-Tac-Toe" };
    for (;;) {
        vga_statusbar(" GAMES   Up/Down select   Enter play   q back");
        int s = menu_pick("Games", g, 4);
        if (s < 0) return;
        if      (s == 0) snake_run();
        else if (s == 1) g2048_run();
        else if (s == 2) tetris_run();
        else             ttt_run();
    }
}

static void menu_tools(void) {
    static const char *g[] = { "Todo", "System Monitor", "Units Converter", "Stopwatch",
        "Base Converter", "Password Generator", "Calendar", "Contacts", "Bookmarks",
        "Timer", "Counter", "Expenses", "Media Player", "World Clock", "Net Tools",
        "Disk Usage", "Kanban", "Notes", "Finance", "Tip Calc", "BMI", "Habits",
        "Date Calc", "Roman", "Cipher", "Morse", "Primes",
        "Percentage", "Text Stats", "Num to Words", "Pomodoro", "Times Table",
        "GCD / LCM", "Factorial", "Fibonacci", "Reverse Text", "Lorem Ipsum",
        "Magic 8-Ball", "Discount", "Currency", "Savings Goal", "Color Picker",
        "Binary Clock", "Pig Latin", "Water Tracker", "Leetspeak", "Piano", "Backups" };
    for (;;) {
        vga_statusbar(" TOOLS   Up/Down select   Enter open   q back");
        int s = menu_pick("Tools", g, 48);
        if (s < 0) return;
        if      (s == 0)  todo_run();
        else if (s == 1)  monitor_run();
        else if (s == 2)  units_run();
        else if (s == 3)  stopwatch_run();
        else if (s == 4)  baseconv_run();
        else if (s == 5)  passgen_run();
        else if (s == 6)  calendar_run();
        else if (s == 7)  contacts_run();
        else if (s == 8)  bookmarks_run();
        else if (s == 9)  timer_run();
        else if (s == 10) counter_run();
        else if (s == 11) expenses_run();
        else if (s == 12) media_run();
        else if (s == 13) worldclock_run();
        else if (s == 14) nettools_run();
        else if (s == 15) diskusage_run();
        else if (s == 16) kanban_run();
        else if (s == 17) notes_run();
        else if (s == 18) finance_run();
        else if (s == 19) tip_run();
        else if (s == 20) bmi_run();
        else if (s == 21) habits_run();
        else if (s == 22) datecalc_run();
        else if (s == 23) roman_run();
        else if (s == 24) cipher_run();
        else if (s == 25) morse_run();
        else if (s == 26) primes_run();
        else if (s == 27) percent_run();
        else if (s == 28) textstats_run();
        else if (s == 29) num2words_run();
        else if (s == 30) pomodoro_run();
        else if (s == 31) timestable_run();
        else if (s == 32) gcdlcm_run();
        else if (s == 33) factorial_run();
        else if (s == 34) fibonacci_run();
        else if (s == 35) reverse_run();
        else if (s == 36) lorem_run();
        else if (s == 37) eightball_run();
        else if (s == 38) discount_run();
        else if (s == 39) currency_run();
        else if (s == 40) savings_run();
        else if (s == 41) colorpick_run();
        else if (s == 42) binclock_run();
        else if (s == 43) piglatin_run();
        else if (s == 44) water_run();
        else if (s == 45) leet_run();
        else if (s == 46) piano_run();
        else              backup_run();
    }
}

static void calc_app_run(void) {
    vga_clear();
    vga_setcolor(VGA_YELLOW, VGA_BLACK); kputs(" Calculator\n");
    vga_setcolor(VGA_DGREY, VGA_BLACK);  kputs("  Type math - symbols or words:  14 times 25   512 divided by 2   5 squared\n");
    kputs("  Also: + - * / %, ^ (power), sqrt, parentheses.   Esc or q to quit.\n\n");
    vga_setcolor(VGA_LGREY, VGA_BLACK);
    char buf[64];
    for (;;) {
        kputs("  > ");
        int len = 0; buf[0] = 0; int sx, sy; vga_getcursor(&sx, &sy); int quit = 0;
        for (;;) {
            char c = keyboard_getc();
            if (c == '\n' || c == '\r') { buf[len] = 0; kputs("\n"); break; }
            else if (c == 27) { quit = 1; break; }
            else if (c == '\b') { if (len) { len--; vga_cell(sx+len, sy, ' ', VGA_LGREY, VGA_BLACK); vga_setcursor(sx+len, sy); } }
            else if (c >= 32 && c < 127 && len < 63) { buf[len++] = c; vga_cell(sx+len-1, sy, c, VGA_LGREY, VGA_BLACK); vga_setcursor(sx+len, sy); }
        }
        if (quit) break;
        if (len == 1 && (buf[0]=='q' || buf[0]=='Q')) break;
        if (len == 0) continue;
        long r = calc_eval(buf);
        vga_setcolor(VGA_LCYAN, VGA_BLACK); kprintf("    = %d\n", (int)r); vga_setcolor(VGA_LGREY, VGA_BLACK);
    }
    vga_statusbar(" type 'menu' for Home    help  commands");
    vga_clear(); vga_setcolor(VGA_LCYAN, VGA_BLACK); kputs("Calculator closed.\n"); vga_setcolor(VGA_LGREY, VGA_BLACK);
}

/* ============================================================================
 * System management: config, the first-boot setup wizard, update & factory reset
 * (SYS_REVISION + OS identity live in version.h, shared with the agent descriptor)
 * ==========================================================================*/

/* accent theme name <-> chrome background, shared by the wizard and config */
static const char  *THEME_NAMES[] = { "blue","green","cyan","red","magenta","grey","amber" };
static const uint8_t THEME_COLS[]  = { VGA_BLUE,VGA_GREEN,VGA_CYAN,VGA_RED,VGA_MAGENTA,VGA_DGREY,VGA_BROWN };
#define N_THEMES 7
static int theme_index(const char *name) {
    for (int i = 0; i < N_THEMES; i++) if (!strcmp(name, THEME_NAMES[i])) return i;
    return 0;
}

/* the option lists the setup wizard offers (Arch-style breadth, friendly wording) */
static const char *OPT_LANG[]    = { "English", "Espanol", "Francais", "Deutsch", "Portugues", "Italiano" };
static const char *VAL_LANG[]    = { "en", "es", "fr", "de", "pt", "it" };
static const char *OPT_KEYMAP[]  = { "US (QWERTY)", "UK", "German", "French (AZERTY)", "Dvorak", "Colemak" };
static const char *VAL_KEYMAP[]  = { "us", "uk", "de", "fr", "dvorak", "colemak" };
static const char *OPT_TZ[]      = { "UTC", "US Eastern", "US Central", "US Mountain", "US Pacific",
                                     "London", "Central Europe", "India", "China", "Japan", "Sydney" };
static const char *VAL_TZ[]      = { "UTC+0","UTC-5","UTC-6","UTC-7","UTC-8","UTC+0","UTC+1","UTC+5:30","UTC+8","UTC+9","UTC+10" };
static const char *OPT_TIMEFMT[] = { "24-hour", "12-hour (AM/PM)" };
static const char *VAL_TIMEFMT[] = { "24", "12" };
static const char *OPT_YN[]      = { "Off  (recommended)", "On" };
static const char *OPT_AGENT[]   = { "Allow  (publish agent.json)", "Block" };

/* a non-cryptographic hash so the passcode isn't stored in plaintext (djb2) */
static uint32_t djb2(const char *s) { uint32_t h = 5381; while (*s) h = ((h << 5) + h) ^ (uint8_t)*s++; return h; }

/* read a key=value line from /etc/system.cfg into out; returns 1 if found */
static int cfg_get(const char *key, char *out, int max) {
    fs_file_t *f = fs_find("/etc/system.cfg");
    if (!f || !f->data) return 0;
    int klen = (int)strlen(key);
    char line[96]; int li = 0;
    for (size_t i = 0; i <= f->len; i++) {
        char c = (i < f->len) ? (char)f->data[i] : '\n';
        if (c == '\n') {
            line[li] = 0;
            if (li > klen && line[klen] == '=' && !strncmp(line, key, (size_t)klen)) {
                int o = 0; for (int k = klen + 1; line[k] && o < max - 1; k++) out[o++] = line[k];
                out[o] = 0; return 1;
            }
            li = 0;
        } else if (li < 95) line[li++] = c;
    }
    return 0;
}
static void cfg_hostname(char *out, int max) {
    if (!cfg_get("hostname", out, max) || !out[0]) strcpy(out, "hisoka");
}
static void apply_saved_theme(void) {
    char t[16]; if (!cfg_get("theme", t, sizeof t)) strcpy(t, "blue");
    vga_set_theme(VGA_WHITE, THEME_COLS[theme_index(t)]);
}

/* set a key=value in /etc/system.cfg (replace the line, or append it) */
static void cfg_set(const char *key, const char *val) {
    fs_file_t *f = fs_find("/etc/system.cfg");
    char buf[512]; int o = 0; int klen = (int)strlen(key); int replaced = 0;
    if (f && f->data) {
        char line[96]; int li = 0;
        for (size_t i = 0; i <= f->len; i++) {
            char c = (i < f->len) ? (char)f->data[i] : '\n';
            if (c == '\n') {
                line[li] = 0;
                if (li > klen && line[klen] == '=' && !strncmp(line, key, (size_t)klen)) {
                    for (const char *p = key; *p && o < 500; p++) buf[o++] = *p; buf[o++] = '=';
                    for (const char *p = val; *p && o < 500; p++) buf[o++] = *p; buf[o++] = '\n';
                    replaced = 1;
                } else if (li > 0 || (i < f->len)) { for (int k = 0; k < li && o < 500; k++) buf[o++] = line[k]; buf[o++] = '\n'; }
                li = 0;
            } else if (li < 95) line[li++] = c;
        }
    }
    if (!replaced) { for (const char *p = key; *p && o < 500; p++) buf[o++] = *p; buf[o++] = '='; for (const char *p = val; *p && o < 500; p++) buf[o++] = *p; buf[o++] = '\n'; }
    fs_write("/etc/system.cfg", buf, (size_t)o);
}

/* read a control file's first line (trimmed) into out */
static void ctl_value(const char *path, char *out, int max) {
    fs_file_t *f = fs_find(path); out[0] = 0;
    if (!f || !f->data) return;
    int o = 0;
    for (size_t i = 0; i < f->len && o < max-1; i++) { char c = (char)f->data[i]; if (c=='\n'||c=='\r') break; if (c==' '&&o==0) continue; out[o++] = c; }
    while (o > 0 && out[o-1] == ' ') o--;
    out[o] = 0;
}

/* fired after ANY file write: apply /sys control files live, and log the write */
static int g_inhook;
static void on_fs_write(const char *path) {
    if (g_inhook) return;
    if (path[1]=='p'&&path[2]=='r'&&path[3]=='o'&&path[4]=='c') return;   /* /proc is auto-generated */
    g_inhook = 1;
    if (!strcmp(path, "/sys/theme")) {
        char v[16]; ctl_value("/sys/theme", v, sizeof v);
        if (v[0]) { vga_set_theme(VGA_WHITE, THEME_COLS[theme_index(v)]); cfg_set("theme", v); }
    } else if (!strcmp(path, "/sys/hostname")) {
        char v[40]; ctl_value("/sys/hostname", v, sizeof v);
        if (v[0]) { cfg_set("hostname", v); char hn[42]; int n=0; for (const char*p=v;*p&&n<40;p++) hn[n++]=*p; hn[n++]='\n'; hn[n]=0; fs_write("/etc/hostname", hn, (size_t)n); }
    } else if (!strcmp(path, "/sys/title")) {
        char v[64]; ctl_value("/sys/title", v, sizeof v); if (v[0]) vga_titlebar(v);
    } else if (!strcmp(path, "/etc/system.cfg")) {
        apply_saved_theme();
    }
    /* log every write (not the log itself) */
    if (strncmp(path, "/var/log", 8)) { char m[96]; int n=0; for (const char*p="wrote ";*p;p++) m[n++]=*p; for (const char*p=path;*p&&n<94;p++) m[n++]=*p; m[n]=0; klog(m); }
    g_inhook = 0;
}
/* everything the setup wizard collects */
typedef struct {
    char host[32], fullname[40], user[32], passhash[12];
    int  lang, keymap, tz, timefmt, theme, telemetry, agent;
} setup_cfg_t;

static void write_config(const setup_cfg_t *c) {
    char buf[512]; int o = 0;
    #define PUT(s) do { for (const char *p = (s); *p && o < 510; p++) buf[o++] = *p; } while (0)
    PUT("# HisokaOS configuration\n");
    PUT("hostname=");  PUT(c->host);
    PUT("\nfullname="); PUT(c->fullname);
    PUT("\nuser=");     PUT(c->user);
    PUT("\npasshash="); PUT(c->passhash);
    PUT("\ntheme=");    PUT(THEME_NAMES[c->theme]);
    PUT("\nlang=");     PUT(VAL_LANG[c->lang]);
    PUT("\nkeymap=");   PUT(VAL_KEYMAP[c->keymap]);
    PUT("\ntimezone="); PUT(VAL_TZ[c->tz]);
    PUT("\ntimefmt=");  PUT(VAL_TIMEFMT[c->timefmt]);
    PUT("\ntelemetry="); PUT(c->telemetry ? "on" : "off");
    PUT("\nagent=");     PUT(c->agent ? "on" : "off");
    PUT("\nshell=hsh\n");
    #undef PUT
    buf[o] = 0;
    fs_write("/etc/system.cfg", buf, (size_t)o);
    char hn[42]; int n = 0; for (const char *p = c->host; *p && n < 40; p++) hn[n++] = *p; hn[n++] = '\n'; hn[n] = 0;
    fs_write("/etc/hostname", hn, (size_t)n);
}

/* the system revision recorded on disk (1 = pre-revision systems) */
static int disk_revision(void) {
    fs_file_t *f = fs_find("/System/revision");
    if (!f || !f->data || !f->len) return 1;
    int n = 0; for (size_t i = 0; i < f->len; i++) { char c = (char)f->data[i]; if (c < '0' || c > '9') break; n = n*10 + (c-'0'); }
    return n ? n : 1;
}
static void write_revision(int r) {
    char b[12]; int n = 0;
    if (r == 0) b[n++] = '0';
    else { char t[12]; int m = 0, x = r; while (x) { t[m++] = (char)('0' + x % 10); x /= 10; } while (m) b[n++] = t[--m]; }
    b[n++] = '\n'; b[n] = 0;
    fs_write("/System/revision", b, (size_t)n);
}

/* the five-line logo, shared by the setup wizard and reset screens */
static void shell_logo(uint8_t fg) {
    vga_setcolor(fg, VGA_BLACK);
    kputs("    __  ___                __         ____  _____\n");
    kputs("   / / / (_)________  ____/ /_____ _ / __ \\/ ___/\n");
    kputs("  / /_/ / / ___/ __ \\/ __  / __/ // // / / /\\__ \\ \n");
    kputs(" / __  / (__  ) /_/ / /_/ / /_/ ,< // /_/ /___/ / \n");
    kputs("/_/ /_/_/____/\\____/\\__,_/\\__/_/|_| \\____//____/  \n");
}
static void busy_ticks(uint32_t n) { uint32_t t = pit_ticks(); while (pit_ticks() - t < n) __asm__ volatile("hlt"); }

static void wiz_header(const char *step) {
    vga_clear();
    shell_logo(VGA_LGREEN);
    vga_setcolor(VGA_YELLOW, VGA_BLACK); kprintf("\n  Setup  -  %s\n\n", step);
    vga_setcolor(VGA_LGREY, VGA_BLACK);
}

static void hex32(uint32_t v, char *out) {
    const char *h = "0123456789abcdef";
    for (int i = 0; i < 8; i++) out[i] = h[(v >> ((7 - i) * 4)) & 15];
    out[8] = 0;
}

/* full-screen single-choice picker used throughout the wizard; returns chosen index */
static int wiz_pick(const char *step, const char *prompt, const char *opts[], int n, int sel) {
    for (;;) {
        wiz_header(step);
        vga_setcolor(VGA_LGREY, VGA_BLACK); kputs("  "); kputs(prompt); kputs("\n");
        vga_setcolor(VGA_DGREY, VGA_BLACK); kputs("  Up/Down to move, Enter to choose.\n");
        vga_setcolor(VGA_LGREY, VGA_BLACK);
        for (int i = 0; i < n; i++) {
            int y = 12 + i;
            uint8_t fg = (i==sel)?VGA_BLACK:VGA_LGREY, bg = (i==sel)?VGA_LGREY:VGA_BLACK;
            for (int x = 4; x <= 48; x++) vga_cell(x, y, ' ', fg, bg);
            vga_cell(5, y, (i==sel)?'>':' ', fg, bg);
            for (int k = 0; opts[i][k] && k < 42; k++) vga_cell(7 + k, y, opts[i][k], fg, bg);
        }
        vga_statusbar(" SETUP   Up/Down: move   Enter: choose");
        vga_setcursor(0, 24);
        char c = keyboard_getc();
        if      (c == 0x10) sel = (sel + n - 1) % n;
        else if (c == 0x0E) sel = (sel + 1) % n;
        else if (c == '\n') return sel;
    }
}

/* masked text entry (passcode) - echoes '*' */
static void masked_input(const char *prompt, char *out, int max) {
    vga_setcolor(VGA_WHITE, VGA_BLACK); kputs(prompt);
    int len = 0; out[0] = 0; int sx, sy; vga_getcursor(&sx, &sy);
    for (;;) {
        char c = keyboard_getc();
        if (c == '\n' || c == '\r') { out[len] = 0; kputs("\n"); return; }
        else if (c == 27) { out[0] = 0; kputs("\n"); return; }
        else if (c == '\b') { if (len) { len--; vga_cell(sx+len, sy, ' ', VGA_WHITE, VGA_BLACK); vga_setcursor(sx+len, sy); } }
        else if (c >= 32 && c < 127 && len < max-1) { out[len++] = c; vga_cell(sx+len-1, sy, '*', VGA_WHITE, VGA_BLACK); vga_setcursor(sx+len, sy); }
    }
}

/* first-boot setup wizard: Arch-style breadth, friendly wording, writes persistent config */
static void setup_wizard(void) {
    setup_cfg_t c; memset(&c, 0, sizeof c);
    strcpy(c.host, "hisoka"); strcpy(c.user, "user"); strcpy(c.passhash, "0");

    wiz_header("Welcome");
    kputs("  Welcome to HisokaOS - a from-scratch 32-bit operating system.\n");
    kputs("  Let's configure your device. Use the arrow keys and Enter.\n\n");
    vga_setcolor(VGA_DGREY, VGA_BLACK); kputs("  Press Enter to begin...");
    vga_setcolor(VGA_LGREY, VGA_BLACK); keyboard_getc();

    c.lang    = wiz_pick("Language",    "Choose your language:",            OPT_LANG,    6, 0);
    c.keymap  = wiz_pick("Keyboard",    "Choose your keyboard layout:",     OPT_KEYMAP,  6, 0);
    c.tz      = wiz_pick("Time zone",   "Choose your region / time zone:",  OPT_TZ,     11, 0);
    c.timefmt = wiz_pick("Time format", "How should the clock read?",       OPT_TIMEFMT, 2, 0);

    wiz_header("Device name");
    kputs("  What should we call this device? It becomes your hostname.\n\n");
    menu_input("  Device name [hisoka]: ", c.host, sizeof c.host);
    if (!c.host[0]) strcpy(c.host, "hisoka");

    wiz_header("Full name");
    kputs("  Your full name (used to personalize the system).\n\n");
    menu_input("  Full name: ", c.fullname, sizeof c.fullname);

    wiz_header("User account");
    kputs("  Pick a short username for your account.\n\n");
    menu_input("  User name [user]: ", c.user, sizeof c.user);
    if (!c.user[0]) strcpy(c.user, "user");

    wiz_header("Passcode");
    kputs("  Set a passcode to lock the device at boot.\n");
    vga_setcolor(VGA_DGREY, VGA_BLACK); kputs("  Leave blank for no login. Stored only as a hash.\n\n");
    vga_setcolor(VGA_LGREY, VGA_BLACK);
    { char pass[24]; masked_input("  Passcode: ", pass, sizeof pass);
      if (pass[0]) { char hx[9]; hex32(djb2(pass), hx); strcpy(c.passhash, hx); } }

    for (;;) {
        wiz_header("Accent color");
        kputs("  Pick an accent color for the title bars and borders.\n");
        vga_setcolor(VGA_DGREY, VGA_BLACK); kputs("  Left/Right to change, Enter to accept.\n\n");
        vga_setcolor(VGA_LCYAN, VGA_BLACK); kprintf("       <   %s   >\n", THEME_NAMES[c.theme]);
        vga_setcolor(VGA_LGREY, VGA_BLACK);
        vga_set_theme(VGA_WHITE, THEME_COLS[c.theme]);
        vga_statusbar(" SETUP   Left/Right: color   Enter: accept");
        vga_setcursor(0, 24);
        char k = keyboard_getc();
        if      (k == 0x02) c.theme = (c.theme + N_THEMES - 1) % N_THEMES;
        else if (k == 0x06) c.theme = (c.theme + 1) % N_THEMES;
        else if (k == '\n') break;
    }

    c.telemetry = wiz_pick("Privacy", "Share anonymous usage data?", OPT_YN, 2, 0);
    c.agent     = (wiz_pick("AI agents", "Let AI agents read system info (agent.json)?", OPT_AGENT, 2, 0) == 0) ? 1 : 0;

    wiz_header("Network");
    {
        const uint8_t *ip = net_my_ip();
        if (rtl8139_present()) {
            vga_setcolor(VGA_LGREEN, VGA_BLACK); kputs("  Network adapter detected (RTL8139).\n");
            vga_setcolor(VGA_LGREY, VGA_BLACK);
            kprintf("  IP address : %u.%u.%u.%u  (DHCP / SLIRP)\n", ip[0], ip[1], ip[2], ip[3]);
            kputs("  You're online - 'fetch' and 'browse' will work.\n");
        } else {
            vga_setcolor(VGA_YELLOW, VGA_BLACK); kputs("  No network adapter found - running offline.\n");
            vga_setcolor(VGA_LGREY, VGA_BLACK);
        }
    }
    vga_setcolor(VGA_DGREY, VGA_BLACK); kputs("\n  Press Enter to continue...");
    vga_setcolor(VGA_LGREY, VGA_BLACK); keyboard_getc();

    wiz_header("Review");
    kprintf("  Language    : %s\n", OPT_LANG[c.lang]);
    kprintf("  Keyboard    : %s\n", OPT_KEYMAP[c.keymap]);
    kprintf("  Time zone   : %s  (%s)\n", OPT_TZ[c.tz], VAL_TZ[c.tz]);
    kprintf("  Time format : %s\n", OPT_TIMEFMT[c.timefmt]);
    kprintf("  Device name : %s\n", c.host);
    kprintf("  Full name   : %s\n", c.fullname[0] ? c.fullname : "(none)");
    kprintf("  User        : %s\n", c.user);
    kprintf("  Passcode    : %s\n", strcmp(c.passhash, "0") ? "set" : "none");
    kprintf("  Accent      : %s\n", THEME_NAMES[c.theme]);
    kprintf("  Telemetry   : %s\n", c.telemetry ? "on" : "off");
    kprintf("  AI agents   : %s\n", c.agent ? "allowed" : "blocked");
    vga_setcolor(VGA_DGREY, VGA_BLACK); kputs("\n  Press Enter to apply...");
    vga_setcolor(VGA_LGREY, VGA_BLACK); keyboard_getc();

    write_config(&c);
    fs_write("/etc/.setup-done", "1\n", 2);
    write_revision(SYS_REVISION);
    if (ata_present()) persist_save();

    wiz_header("Finishing");
    kputs("  Applying your settings");
    for (int i = 0; i < 14; i++) { kputs("."); busy_ticks(6); }
    kputs("\n\n");
    vga_setcolor(VGA_LGREEN, VGA_BLACK);
    kprintf("  All set! Welcome to HisokaOS, %s.\n", c.fullname[0] ? c.fullname : c.user);
    vga_setcolor(VGA_LGREY, VGA_BLACK);
    busy_ticks(100);
}

/* if a passcode was set in setup, lock the device at boot until it's entered */
static void login_gate(void) {
    char want[12];
    if (!cfg_get("passhash", want, sizeof want) || !want[0] || !strcmp(want, "0")) return;
    char host[40], user[40];
    cfg_hostname(host, sizeof host);
    if (!cfg_get("user", user, sizeof user) || !user[0]) strcpy(user, "user");
    for (;;) {
        vga_clear();
        shell_logo(VGA_LGREEN);
        vga_setcolor(VGA_LGREY, VGA_BLACK); kprintf("\n  %s@%s\n\n", user, host);
        char pass[24]; masked_input("  Passcode: ", pass, sizeof pass);
        char hx[9]; hex32(djb2(pass), hx);
        if (!strcmp(hx, want)) {
            vga_setcolor(VGA_LGREEN, VGA_BLACK); kputs("\n  Welcome back.\n");
            vga_setcolor(VGA_LGREY, VGA_BLACK); busy_ticks(40); vga_clear(); return;
        }
        vga_setcolor(VGA_LRED, VGA_BLACK); kputs("\n  Incorrect passcode. Try again.\n");
        vga_setcolor(VGA_LGREY, VGA_BLACK); busy_ticks(70);
    }
}

/* the changelog, shown scrollable in a box (starts at the top) */
static const char CHANGELOG[] =
"HisokaOS - what's new\n"
"=====================\n"
"\n"
"rev 8 (security + stability sweep)\n"
"  - Net: fixed out-of-bounds reads in the TCP receive and DNS parser\n"
"    (a malicious/spoofed server could crash the OS or leak memory).\n"
"  - Disk: a corrupted saved image can no longer crash the OS on boot\n"
"    (persist_load length now validated as unsigned).\n"
"  - Heap: double-free guard + backward coalescing (less fragmentation\n"
"    over long 24/7 uptimes).\n"
"  - Files: deleting a folder now removes its contents (no orphaned nodes).\n"
"\n"
"rev 6\n"
"  - Everything is a file: a full tree (/etc /dev /proc /sys /var/log\n"
"    /usr/share/doc) - ~100 real files, all browsable in Files.\n"
"  - Live /proc: memory, network and uptime as files (regenerated on read).\n"
"  - Control via files: write /sys/theme or /sys/hostname and the OS\n"
"    changes live (like Linux /sys).\n"
"  - Logs for everything: commands and file writes -> the System Log\n"
"    (the 'log' command opens it in a scrollable box).\n"
"  - Settings: a clear-temp button.\n"
"\n"
"rev 5\n"
"  - Polished, boxed UIs (a shared TUI toolkit): framed panels,\n"
"    tiles and a scrollable changelog - no more plain text.\n"
"  - This changelog, in a scrollable box.\n"
"\n"
"rev 4\n"
"  - Alexis: your built-in assistant (purple banner, thoughts,\n"
"    /commands), streamed from the host model.\n"
"  - Smart calculator: words too ('14 times 25', '5 squared').\n"
"  - 12 real utility apps: Todo, Monitor, Units, Stopwatch, Base,\n"
"    Passwords, Calendar, Contacts, Bookmarks, Timer, Counter, Expenses.\n"
"  - Image viewer (real pixels), file converter (fcv), download.\n"
"  - Build: a VS Code-style editor with a file sidebar.\n"
"\n"
"rev 3\n"
"  - Everything-is-a-file: apps live in /Applications, 'open' launches them.\n"
"  - Setup wizard, update mechanism, factory reset, passcode login.\n"
"  - Boot screen first; pager; 'help' shows everything.\n"
"\n"
"rev 2 and earlier\n"
"  - The kernel: paging, scheduler, drivers, a from-scratch TCP/IP\n"
"    stack, an in-RAM filesystem with disk persistence, graphics mode,\n"
"    a streamed Chromium browser, and the apps and games.\n";

static void cmd_changelog(void) { ui_scrollbox_view("Changelog - HisokaOS", CHANGELOG); }

/* update: compare the running build's system revision to the one saved on disk.
 * This is honest about what an in-VM OS can self-update: the on-disk system files
 * and revision. The kernel image itself is produced on the host build machine. */
static void cmd_update(const char *args) {
    int have = disk_revision(), avail = SYS_REVISION;
    if (!strcmp(args, "apply") || !strcmp(args, "now") || !strcmp(args, "install")) {
        if (have >= avail) { kprintf("update: already up to date (revision %d)\n", have); return; }
        kprintf("update: staging system revision %d -> %d ...\n", have, avail);
        write_revision(avail);
        fs_write("/System/version.txt", "HisokaOS 0.2 (i386)\nbuild: from-scratch C kernel\n", 47);
        fs_write("/System/update.pending", "1\n", 2);
        if (ata_present()) persist_save();
        kputs("update: staged. restarting to finish installation...\n");
        busy_ticks(80);
        screen_message("Restarting to install update...", 1);   /* reboot -> boot update screen */
        return;
    }
    cmd_changelog();      /* show the boxed, scrollable changelog (starts at the top) */
    vga_setcolor(VGA_LCYAN, VGA_BLACK); kputs("HisokaOS Update\n");
    vga_setcolor(VGA_LGREY, VGA_BLACK);
    kprintf("  installed system revision : %d\n", have);
    kprintf("  this build provides       : %d\n", avail);
    if (avail > have) {
        vga_setcolor(VGA_YELLOW, VGA_BLACK);
        kprintf("  update available: revision %d -> %d\n", have, avail);
        vga_setcolor(VGA_LGREY, VGA_BLACK);
        kputs("  run 'update apply' to install it (the system will restart).\n");
    } else {
        vga_setcolor(VGA_LGREEN, VGA_BLACK); kputs("  HisokaOS is up to date.\n");
        vga_setcolor(VGA_LGREY, VGA_BLACK);
    }
    vga_setcolor(VGA_DGREY, VGA_BLACK);
    kputs("\n  note: the kernel image is built on the host dev machine; 'update'\n");
    kputs("  installs the system revision and refreshes system files on disk.\n");
    vga_setcolor(VGA_LGREY, VGA_BLACK);
}

/* factory reset: wipe the disk image and reboot into a fresh first-boot setup */
static void cmd_reset(const char *args) {
    if (strcmp(args, "confirm") != 0) {
        vga_setcolor(VGA_LRED, VGA_BLACK); kputs("Factory reset\n");
        vga_setcolor(VGA_LGREY, VGA_BLACK);
        kputs("  This erases ALL your files and settings and restores defaults.\n");
        kputs("  The whole disk is wiped. This cannot be undone.\n\n");
        vga_setcolor(VGA_YELLOW, VGA_BLACK); kputs("  To proceed, run:  reset confirm\n");
        vga_setcolor(VGA_LGREY, VGA_BLACK);
        return;
    }
    kputs("factory reset: wiping disk...\n");
    if (ata_present()) persist_format();
    fs_delete("/etc/.setup-done");
    busy_ticks(70);
    screen_message("Restoring factory settings...", 1);   /* reboot -> fresh disk -> setup */
}

/* shown at boot when an update is available: Yes / Don't-ask-again / Later */
static void update_prompt(void) {
    int have = disk_revision(), avail = SYS_REVISION;
    if (avail <= have) return;
    fs_file_t *d = fs_find("/System/update-dismissed");
    if (d && d->data) { int dn = 0; for (size_t i = 0; i < d->len; i++) { char c = (char)d->data[i]; if (c < '0' || c > '9') break; dn = dn*10 + (c-'0'); } if (dn == avail) return; }
    vga_clear();
    ui_panel(14, 4, 52, 13, "System Update", VGA_LCYAN, VGA_BLACK);
    char l[48]; int o = 0; for (const char *p = "An update is available:  rev "; *p; p++) l[o++] = *p;
    { int v = have; char t[6]; int m=0; if(!v)t[m++]='0'; while(v){t[m++]=(char)('0'+v%10);v/=10;} while(m)l[o++]=t[--m]; }
    l[o++] = ' '; l[o++] = '-'; l[o++] = '>'; l[o++] = ' ';
    { int v = avail; char t[6]; int m=0; if(!v)t[m++]='0'; while(v){t[m++]=(char)('0'+v%10);v/=10;} while(m)l[o++]=t[--m]; } l[o] = 0;
    ui_text(17, 6, l, VGA_LGREY, VGA_BLACK);
    ui_text(17, 8,  "Y   Yes, install now  (restarts)", VGA_LGREEN, VGA_BLACK);
    ui_text(17, 9,  "D   Don't ask me again", VGA_DGREY, VGA_BLACK);
    ui_text(17, 10, "L   Remind me later", VGA_DGREY, VGA_BLACK);
    vga_statusbar(" SYSTEM UPDATE   Y install   D dismiss   L later");
    vga_setcursor(0, 24);
    for (;;) {
        char k = keyboard_getc();
        if (k == 'y' || k == 'Y') {
            write_revision(avail);
            fs_write("/System/update.pending", "1\n", 2);
            if (ata_present()) persist_save();
            screen_message("Restarting to install update...", 1);   /* reboot -> boot update screen */
            return;
        } else if (k == 'd' || k == 'D') {
            char b[8]; int n = 0, v = avail; char t[6]; int m = 0; if (!v) t[m++]='0'; while (v) { t[m++]=(char)('0'+v%10); v/=10; } while (m) b[n++]=t[--m]; b[n++]='\n'; b[n]=0;
            fs_write("/System/update-dismissed", b, (size_t)n);
            if (ata_present()) persist_save();
            klog("update: dismissed by user (don't ask again)");
            break;
        } else if (k == 'l' || k == 'L' || k == 27 || k == '\n') {
            klog("update: deferred (remind later)");
            break;
        }
    }
    vga_clear();
}

/* ============================================================================
 * Everything-is-a-file: apps live in /Applications and launch from the file
 * browser (or the 'open' command). Each app is a real file you can see in Files.
 * ==========================================================================*/
static int ieq(const char *a, const char *b) {     /* case-insensitive equality */
    for (;; a++, b++) {
        char x = *a, y = *b;
        if (x >= 'A' && x <= 'Z') x += 32;
        if (y >= 'A' && y <= 'Z') y += 32;
        if (x != y) return 0;
        if (!x) return 1;
    }
}

/* launchers that need a wrapper (prompt for input or reuse a menu screen) */
static void app_editor(void) { build_run(0); }   /* opens to the file sidebar (a menu) */
static void app_sysinfo(void) {
    vga_clear(); cmd_mem();
    const uint8_t *ip = net_my_ip();
    kprintf("Net   : %u.%u.%u.%u (gateway %u.%u.%u.%u)\n", ip[0],ip[1],ip[2],ip[3],
            net_gw_ip()[0],net_gw_ip()[1],net_gw_ip()[2],net_gw_ip()[3]);
    kprintf("Uptime: %u s\n", pit_ticks()/100);
    vga_setcolor(VGA_DGREY, VGA_BLACK); kputs("\n  (any key)\n"); vga_setcolor(VGA_LGREY, VGA_BLACK);
    keyboard_getc();
}
static void app_help(void) { cmd_man(); }

typedef struct { const char *name; void (*run)(void); } app_entry_t;
static const app_entry_t APPS[] = {
    { "Build",        app_editor   }, { "Files",        explorer_run },
    { "Calculator",   calc_app_run }, { "Web Browser",  menu_web     },
    { "Draw",         draw_run     }, { "Clock",        clock_run    },
    { "Settings",     settings_run }, { "Snake",        snake_run    },
    { "2048",         g2048_run    }, { "Tetris",       tetris_run   },
    { "Tic-Tac-Toe",  ttt_run      }, { "System Info",  app_sysinfo  },
    { "Help",         app_help     }, { "Alexis",       alexis_run   },
    { "Todo",         todo_run     }, { "Monitor",      monitor_run  },
    { "Units",        units_run    }, { "Stopwatch",    stopwatch_run},
    { "Base",         baseconv_run }, { "Passwords",    passgen_run  },
    { "Calendar",     calendar_run }, { "Contacts",     contacts_run },
    { "Bookmarks",    bookmarks_run}, { "Timer",        timer_run    },
    { "Counter",      counter_run  }, { "Expenses",     expenses_run },
    { "Media Player", media_run    }, { "World Clock",  worldclock_run },
    { "Net Tools",    nettools_run }, { "Disk Usage",   diskusage_run },
    { "Kanban",       kanban_run   }, { "Notes",        notes_run    },
    { "Finance",      finance_run  }, { "Tip Calc",     tip_run      },
    { "BMI",          bmi_run      }, { "Habits",       habits_run   },
    { "Date Calc",    datecalc_run }, { "Roman",        roman_run    },
    { "Cipher",       cipher_run   }, { "Morse",        morse_run    },
    { "Primes",       primes_run   }, { "Percentage",   percent_run  },
    { "Text Stats",   textstats_run}, { "Num to Words", num2words_run},
    { "Pomodoro",     pomodoro_run }, { "Times Table",  timestable_run },
    { "GCD / LCM",    gcdlcm_run   }, { "Factorial",    factorial_run},
    { "Fibonacci",    fibonacci_run}, { "Reverse Text", reverse_run  },
    { "Lorem Ipsum",  lorem_run    }, { "Magic 8-Ball", eightball_run},
    { "Discount",     discount_run }, { "Currency",     currency_run },
    { "Savings Goal", savings_run  }, { "Color Picker", colorpick_run},
    { "Binary Clock", binclock_run }, { "Pig Latin",    piglatin_run },
    { "Water Tracker",water_run    }, { "Leetspeak",    leet_run     },
    { "Piano",        piano_run    }, { "Backups",      backup_run   },
    { "Hisoka Forum", forum_run    },
};
#define N_APPS ((int)(sizeof(APPS) / sizeof(APPS[0])))

static int app_launch_name(const char *name) {
    for (int i = 0; i < N_APPS; i++) if (ieq(name, APPS[i].name)) {
        char m[64]; int n=0; for (const char*p="launch app: ";*p;p++) m[n++]=*p; for (const char*p=APPS[i].name;*p&&n<62;p++) m[n++]=*p; m[n]=0; klog(m);
        APPS[i].run(); return 1;
    }
    return 0;
}

/* the explorer's open-handler: launch an app, view an image, run a .sh, else edit */
static int open_path(const char *path) {
    const char *base = base_name(path);
    if (is_proc_path(path)) procfs_refresh();   /* /proc files are live */
    if (app_launch_name(base)) return 1;
    if (imgview_is_image(base)) { imgview_run(path); return 1; }   /* real-pixel image */
    int L = (int)strlen(base);
    if (L > 3 && base[L-3] == '.' && base[L-2] == 's' && base[L-1] == 'h') {
        char tmp[FS_NAME_LEN]; strcpy(tmp, path); cmd_run(tmp); return 1;
    }
    build_run(path);   /* every other file opens in the Build editor */
    return 1;
}

/* `open <path-or-app>`: launch an app, run a script, or open a file in the editor */
static void cmd_open(const char *arg) {
    if (!*arg) { kputs("usage: open <file | app name>\n"); return; }
    if (app_launch_name(base_name(arg))) return;   /* `open Calculator` */
    const char *p = P(arg);
    if (app_launch_name(base_name(p))) return;
    fs_file_t *f = fs_find(p);
    if (!f)        { kprintf("open: %s: not found\n", arg); return; }
    if (f->is_dir) { kputs("open: that's a folder - use 'explorer' or 'cd'\n"); return; }
    if (open_path(p)) return;
    edit_run(p);
}

/* create /Applications/<app> for every app so they appear (and launch) in Files */
static void app_seed(void) {
    fs_mkdir("/Applications");
    for (int i = 0; i < N_APPS; i++) {
        char path[FS_NAME_LEN]; int o = 0;
        for (const char *p = "/Applications/"; *p; p++) path[o++] = *p;
        for (const char *p = APPS[i].name; *p && o < FS_NAME_LEN - 1; p++) path[o++] = *p;
        path[o] = 0;
        if (fs_find(path)) continue;
        char body[96]; int b = 0;
        for (const char *p = "HisokaOS app: "; *p; p++) body[b++] = *p;
        for (const char *p = APPS[i].name; *p; p++) body[b++] = *p;
        for (const char *p = "\nOpen this from Files (Enter) to launch it.\n"; *p; p++) body[b++] = *p;
        body[b] = 0;
        fs_write(path, body, (size_t)b);
    }
}

/* download a file off the internet straight into a folder (e.g. Downloads/Pictures) */
static void cmd_download(char *args) {
    if (!rtl8139_present()) { kputs("download: no network device\n"); return; }
    char *dest = split(args);                 /* args = url, dest = optional path */
    if (!*args) { kputs("usage: download <url> [dest]\n  e.g.  download example.com/cat.bmp /home/Pictures/cat.bmp\n"); return; }
    static char body[16384];
    char host[64], path[96];
    parse_url(args, host, path);
    kprintf("downloading http://%s%s ...\n", host, path);
    int n = net_http_get(host, path, body, sizeof(body));
    if (n <= 0) { kprintf("  download failed (error %d)\n", n); return; }
    char *data = body; int dlen = n;          /* skip past the HTTP headers */
    for (int i = 0; i + 3 < n; i++)
        if (body[i]=='\r' && body[i+1]=='\n' && body[i+2]=='\r' && body[i+3]=='\n') { data = &body[i+4]; dlen = n - (i+4); break; }
    if (dlen < 0) dlen = 0;
    char out[FS_NAME_LEN];
    if (*dest) strcpy(out, P(dest));
    else {
        const char *bn = path; for (const char *p = path; *p; p++) if (*p == '/') bn = p + 1;
        if (!*bn) bn = "index.html";
        int o = 0; for (const char *p = "/home/Downloads/"; *p; p++) out[o++] = *p;
        for (const char *p = bn; *p && o < FS_NAME_LEN - 1; p++) out[o++] = *p; out[o] = 0;
    }
    if (fs_write(out, data, (size_t)dlen) == 0) kprintf("saved %d bytes to %s\n", dlen, out);
    else kprintf("download: could not write %s (filesystem full?)\n", out);
}

static void cmd_which(const char *name) {
    for (int i = 0; CMDS[i]; i++) if (!strcmp(CMDS[i], name)) { kprintf("%s: shell built-in\n", name); return; }
    fs_file_t *f = fs_find(P(name)); if (f && !f->is_dir) { kprintf("%s\n", f->name); return; }
    kprintf("%s: not found\n", name);
}
static void cmd_mousetest(void) {
    if (!mouse_present()) { kputs("mouse: driver not present\n"); return; }
    vga_clear();
    ui_panel(2, 1, 76, 22, "Mouse Test", VGA_LCYAN, VGA_BLACK);
    ui_text(5, 3, "Move the mouse and click. Press any key to quit.", VGA_DGREY, VGA_BLACK);
    int px = -1, py = -1;
    for (;;) {
        int x, y, b; mouse_get(&x, &y, &b);
        if (px >= 0 && (px != x || py != y)) vga_cell(px, py, ' ', VGA_LGREY, VGA_BLACK);
        char s[24]; int o = 0; for (const char *p = "pos "; *p; p++) s[o++] = *p;
        s[o++] = (char)('0'+x/10); s[o++] = (char)('0'+x%10); s[o++] = ','; s[o++] = (char)('0'+y/10); s[o++] = (char)('0'+y%10); s[o] = 0;
        ui_text(5, 6, s, VGA_WHITE, VGA_BLACK);
        ui_text(5, 8, "L", (b&1)?VGA_LRED:VGA_DGREY, VGA_BLACK);
        ui_text(8, 8, "R", (b&2)?VGA_LCYAN:VGA_DGREY, VGA_BLACK);
        ui_text(11, 8, "M", (b&4)?VGA_LGREEN:VGA_DGREY, VGA_BLACK);
        vga_cell(x, y, (char)0xDB, (b&1)?VGA_LRED:VGA_LGREEN, VGA_BLACK);
        px = x; py = y;
        if (keyboard_trygetc()) break;
        for (volatile int i = 0; i < 150000; i++) __asm__ volatile("nop");
    }
    vga_clear();
}
static void cmd_id(void) {
    char u[40]; if (!cfg_get("user", u, sizeof u) || !u[0]) strcpy(u, "user");
    if (is_admin()) kprintf("uid=0(%s) gid=0(admin) groups=0(admin),10(wheel),4(users)\n", u);
    else            kprintf("uid=1000(%s) gid=1000(%s) groups=1000(%s),4(users)\n", u, u, u);
}
/* printf: interpret \n \t \\ escapes, no trailing newline (like the real thing) */
static void cmd_printf(const char *s) {
    for (const char *p = s; *p; p++) {
        if (*p == '\\' && p[1]) {
            p++;
            if      (*p == 'n') kputs("\n");
            else if (*p == 't') kputs("\t");
            else if (*p == '\\') kputs("\\");
            else { char b[2] = { *p, 0 }; kputs(b); }
        } else { char b[2] = { *p, 0 }; kputs(b); }
    }
}
/* base64-encode the argument text */
static void cmd_base64(const char *s) {
    static const char *T = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int n = (int)strlen(s);
    for (int i = 0; i < n; i += 3) {
        int b0 = (uint8_t)s[i], b1 = i+1 < n ? (uint8_t)s[i+1] : 0, b2 = i+2 < n ? (uint8_t)s[i+2] : 0;
        char o[5];
        o[0] = T[b0 >> 2];
        o[1] = T[((b0 & 3) << 4) | (b1 >> 4)];
        o[2] = i+1 < n ? T[((b1 & 15) << 2) | (b2 >> 6)] : '=';
        o[3] = i+2 < n ? T[b2 & 63] : '=';
        o[4] = 0; kputs(o);
    }
    kputs("\n");
}
/* sum: a 16-bit checksum and block count of a file (like BSD 'sum') */
static void cmd_sum(const char *path) {
    fs_file_t *f = fs_find(path);
    if (!f || f->is_dir) { kprintf("sum: %s: not found\n", path); return; }
    uint32_t s = 0; for (size_t i = 0; i < f->len; i++) s = (s + f->data[i]) & 0xFFFF;
    kprintf("%u %u %s\n", s, (uint32_t)((f->len + 511) / 512), base_name(path));
}
/* strings: print runs of >=4 printable bytes */
static void cmd_strings(const char *path) {
    fs_file_t *f = fs_find(path); if (!f || f->is_dir) { kprintf("strings: %s: not found\n", path); return; }
    char run[80]; int rl = 0;
    for (size_t i = 0; i <= f->len; i++) { char c = (i < f->len) ? (char)f->data[i] : 0;
        if (c >= 32 && c < 127 && rl < 79) run[rl++] = c;
        else { if (rl >= 4) { run[rl] = 0; kprintf("%s\n", run); } rl = 0; } }
}
/* od: a hex dump with offsets */
static void cmd_od(const char *path) {
    fs_file_t *f = fs_find(path); if (!f || f->is_dir) { kprintf("od: %s: not found\n", path); return; }
    for (size_t i = 0; i < f->len; i += 16) {
        kprintf("%x ", (uint32_t)i);
        for (size_t j = i; j < i+16 && j < f->len; j++) { uint8_t b = f->data[j]; kprintf(" %s%x", b<16?"0":"", b); }
        kputs("\n");
    }
}
/* fold: wrap lines at 64 columns */
static void cmd_fold(const char *path) {
    fs_file_t *f = fs_find(path); if (!f || f->is_dir) { kprintf("fold: %s: not found\n", path); return; }
    int col = 0;
    for (size_t i = 0; i < f->len; i++) { char c = (char)f->data[i];
        if (c == '\n') { kputs("\n"); col = 0; continue; }
        char b[2] = { c, 0 }; kputs(b); if (++col >= 64) { kputs("\n"); col = 0; } }
    kputs("\n");
}
/* cksum: a CRC32 + byte count (like coreutils cksum, different polynomial layout) */
static void cmd_cksum(const char *path) {
    fs_file_t *f = fs_find(path); if (!f || f->is_dir) { kprintf("cksum: %s: not found\n", path); return; }
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < f->len; i++) { crc ^= f->data[i]; for (int k = 0; k < 8; k++) crc = (crc >> 1) ^ (0xEDB88320u & (uint32_t)(-(int)(crc & 1))); }
    crc = ~crc;
    kprintf("%u %u %s\n", crc, (uint32_t)f->len, base_name(path));
}

/* cut -c A-B file  (character range, 1-based)  |  cut -f N [-d D] file  (field N) */
static void cmd_cut(const char *args) {
    char delim = '\t', path[FS_NAME_LEN]; path[0] = 0;
    int mode = 0, field = 0, cstart = 0, cend = 0;   /* mode: 1=chars 2=fields */
    for (const char *p = args; *p; ) {
        while (*p == ' ') p++;
        if (!*p) break;
        if (p[0] == '-' && p[1] == 'c') {
            p += 2; while (*p == ' ') p++; mode = 1;
            cstart = 0; while (*p >= '0' && *p <= '9') cstart = cstart*10 + (*p++ - '0');
            if (*p == '-') { p++; cend = 0; while (*p >= '0' && *p <= '9') cend = cend*10 + (*p++ - '0'); }
            else cend = cstart;
        } else if (p[0] == '-' && p[1] == 'f') {
            p += 2; while (*p == ' ') p++; mode = mode ? mode : 2;
            field = 0; while (*p >= '0' && *p <= '9') field = field*10 + (*p++ - '0');
        } else if (p[0] == '-' && p[1] == 'd') {
            p += 2; while (*p == ' ') p++; if (*p) delim = *p++;
        } else {
            int k = 0; while (*p && *p != ' ' && k < FS_NAME_LEN-1) path[k++] = *p++; path[k] = 0;
        }
    }
    if (!mode || !path[0]) { kputs("usage: cut -c A-B file | cut -f N [-d D] file\n"); return; }
    fs_file_t *f = fs_find(path);
    if (!f || f->is_dir) { kprintf("cut: %s: not found\n", path); return; }
    size_t i = 0;
    while (i < f->len) {
        size_t ls = i; while (i < f->len && f->data[i] != '\n') i++;
        if (mode == 1) {
            for (int c = cstart; c <= cend; c++) { size_t idx = ls + (size_t)(c-1);
                if (c >= 1 && idx < i) { char b[2] = { (char)f->data[idx], 0 }; kputs(b); } }
        } else {
            int fn = 1; size_t fstart = ls;
            for (size_t j = ls; j <= i; j++)
                if (j == i || f->data[j] == (uint8_t)delim) {
                    if (fn == field) for (size_t k = fstart; k < j; k++) { char b[2] = { (char)f->data[k], 0 }; kputs(b); }
                    fn++; fstart = j + 1;
                }
        }
        kputs("\n");
        if (i < f->len && f->data[i] == '\n') i++;
    }
}

/* ---- the "current file" the terminal is focused on (set by Files' 't' key) ---- */
static char g_curfile[FS_NAME_LEN];
static void set_current_file(const char *path) {
    strcpy(g_curfile, path);
    vga_statusbar(" type 'menu' for Home    help  commands");
    vga_clear();
    vga_setcolor(VGA_LCYAN, VGA_BLACK); kprintf("current file: %s\n", g_curfile);
    vga_setcolor(VGA_DGREY, VGA_BLACK);
    kputs("  convert it with 'fcv <new.ext>'  (e.g. fcv page.html, fcv data.json)\n");
    kputs("  open/edit it with 'build', view an image with 'img'.\n");
    vga_setcolor(VGA_LGREY, VGA_BLACK);
}

/* ---- fcv: a real text file converter (txt/md/html/json/csv/b64) with a progress bar ---- */
static const char *file_ext(const char *name) { const char *d = ""; for (const char *p = name; *p; p++) if (*p == '.') d = p + 1; return d; }

static void html_escape(const char *s, int n, char *o, int max, int *oi) {
    int k = *oi;
    for (int i = 0; i < n && k < max - 6; i++) {
        char c = s[i];
        if      (c == '<') { o[k++]='&'; o[k++]='l'; o[k++]='t'; o[k++]=';'; }
        else if (c == '>') { o[k++]='&'; o[k++]='g'; o[k++]='t'; o[k++]=';'; }
        else if (c == '&') { o[k++]='&'; o[k++]='a'; o[k++]='m'; o[k++]='p'; o[k++]=';'; }
        else o[k++] = c;
    }
    *oi = k;
}
/* very small markdown -> html: # headings, - bullets, blank-line paragraphs */
static int md_to_html(const char *s, int n, char *o, int max) {
    int k = 0;
    #define PUT(str) do { for (const char *p=(str); *p && k<max-1; p++) o[k++]=*p; } while(0)
    PUT("<html><body>\n");
    int i = 0;
    while (i < n) {
        int e = i; while (e < n && s[e] != '\n') e++;
        int ll = e - i;
        if (ll == 0) { PUT("<br>\n"); }
        else if (s[i] == '#') {
            int h = 0; while (i + h < e && s[i+h] == '#') h++;
            int hl = h > 6 ? 6 : h;
            char tag[5] = { 'h', (char)('0'+hl), 0, 0, 0 };
            int j = i + h; while (j < e && s[j] == ' ') j++;
            PUT("<"); PUT(tag); PUT(">"); html_escape(s+j, e-j, o, max, &k); PUT("</"); PUT(tag); PUT(">\n");
        } else if (s[i] == '-' && i+1 < e && s[i+1] == ' ') {
            PUT("<li>"); html_escape(s+i+2, e-i-2, o, max, &k); PUT("</li>\n");
        } else {
            PUT("<p>"); html_escape(s+i, ll, o, max, &k); PUT("</p>\n");
        }
        i = e + 1;
    }
    PUT("</body></html>\n");
    #undef PUT
    o[k] = 0; return k;
}
/* strip html tags -> plain text */
static int strip_tags(const char *s, int n, char *o, int max) {
    int k = 0, in = 0;
    for (int i = 0; i < n && k < max - 1; i++) {
        char c = s[i];
        if (c == '<') in = 1;
        else if (c == '>') in = 0;
        else if (!in) o[k++] = c;
    }
    o[k] = 0; return k;
}
/* csv -> json array of arrays */
static int csv_to_json(const char *s, int n, char *o, int max) {
    int k = 0;
    #define PUT(str) do { for (const char *p=(str); *p && k<max-1; p++) o[k++]=*p; } while(0)
    PUT("[\n");
    int i = 0, firstrow = 1;
    while (i < n) {
        int e = i; while (e < n && s[e] != '\n') e++;
        if (e > i) {
            if (!firstrow) PUT(",\n");
            firstrow = 0;
            PUT("  [");
            int j = i, firstcol = 1;
            while (j < e) {
                int c = j; while (c < e && s[c] != ',') c++;
                if (!firstcol) PUT(", ");
                firstcol = 0;
                PUT("\""); html_escape(s+j, c-j, o, max, &k); PUT("\"");
                j = c + 1;
            }
            PUT("]");
        }
        i = e + 1;
    }
    PUT("\n]\n");
    #undef PUT
    o[k] = 0; return k;
}
static int to_base64(const char *s, int n, char *o, int max) {
    static const char *T = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int k = 0;
    for (int i = 0; i < n && k < max - 5; i += 3) {
        int b0 = (uint8_t)s[i], b1 = i+1<n?(uint8_t)s[i+1]:0, b2 = i+2<n?(uint8_t)s[i+2]:0;
        o[k++] = T[b0>>2]; o[k++] = T[((b0&3)<<4)|(b1>>4)];
        o[k++] = i+1<n ? T[((b1&15)<<2)|(b2>>6)] : '='; o[k++] = i+2<n ? T[b2&63] : '=';
    }
    o[k] = 0; return k;
}

static void fcv_progress(const char *src, const char *dst) {
    vga_clear();
    vga_setcolor(VGA_LCYAN, VGA_BLACK); kputs(" File Converter\n"); vga_setcolor(VGA_LGREY, VGA_BLACK);
    kprintf("  %s  ->  %s\n\n", base_name(src), base_name(dst));
    int bw = 44, bx = 3, by = 5;
    for (int i = 0; i <= bw; i++) {
        for (int x = 0; x < bw; x++) vga_cell(bx + x, by, ' ', VGA_WHITE, x < i ? VGA_LGREEN : VGA_DGREY);
        int pct = i * 100 / bw;
        char p[5]; int m = 0;
        if (pct >= 100) { p[m++]='1'; p[m++]='0'; p[m++]='0'; } else { if (pct>=10) p[m++]=(char)('0'+pct/10); p[m++]=(char)('0'+pct%10); }
        p[m++]='%'; p[m]=0;
        for (int x = 0; x < 5; x++) vga_cell(bx + bw + 2 + x, by, x < m ? p[x] : ' ', VGA_LGREY, VGA_BLACK);
        busy_ticks(2);
    }
}

static void cmd_fcv(char *args) {
    char *second = split(args);                 /* args = arg1, second = arg2 (maybe empty) */
    const char *src, *dst;
    char srcbuf[FS_NAME_LEN], dstbuf[FS_NAME_LEN];
    if (*second) { strcpy(srcbuf, P(args)); strcpy(dstbuf, P(second)); src = srcbuf; dst = dstbuf; }
    else if (g_curfile[0]) { src = g_curfile; strcpy(dstbuf, P(args)); dst = dstbuf; }
    else { kputs("usage: fcv <source> <dest>   or pick a file in Files with 't' then 'fcv <dest>'\n"); return; }

    fs_file_t *f = fs_find(src);
    if (!f || f->is_dir) { kprintf("fcv: %s: not found\n", base_name(src)); return; }

    static char out[32768];
    const char *de = file_ext(dst);
    int n = (int)(f->len > 30000 ? 30000 : f->len);
    int olen;
    if      (!strcmp(de, "html") || !strcmp(de, "htm")) olen = md_to_html((char *)f->data, n, out, sizeof out);
    else if (!strcmp(de, "txt")  || !strcmp(de, "md"))  olen = strip_tags((char *)f->data, n, out, sizeof out);
    else if (!strcmp(de, "json")) olen = csv_to_json((char *)f->data, n, out, sizeof out);
    else if (!strcmp(de, "b64")  || !strcmp(de, "base64")) olen = to_base64((char *)f->data, n, out, sizeof out);
    else { /* unknown target: reformat as a faithful copy */
        olen = n; for (int i = 0; i < n; i++) out[i] = (char)f->data[i]; out[olen] = 0;
    }

    fcv_progress(src, dst);
    int r = fs_write(dst, out, (size_t)olen);
    vga_setcolor(r == 0 ? VGA_LGREEN : VGA_LRED, VGA_BLACK);
    if (r == 0) kprintf("\n  converted -> %s  (%d bytes)\n", dst, olen);
    else        kprintf("\n  fcv: could not write %s\n", dst);
    vga_setcolor(VGA_DGREY, VGA_BLACK);
    kputs("  press any key...\n"); vga_setcolor(VGA_LGREY, VGA_BLACK);
    keyboard_getc();
    vga_clear();
}

static void menu_run(void) {
    static const char *items[] = {
        "Alexis          - your assistant (ask, code, control the OS)",
        "Settings        - appearance & system info",
        "Files           - browse and open your files",
        "Build           - code editor (syntax + line numbers)",
        "Calculator      - do some math",
        "Web Browser     - open a website",
        "Draw            - paint on a canvas",
        "Clock           - big clock",
        "Tools           - todo, monitor, units, stopwatch & more",
        "Games           - snake, 2048, tetris & more",
        "System Info     - memory, network, uptime",
        "Help / Manual   - everything you can do",
        "Terminal        - drop to the command line",
    };
    const int N = 13;
    int sel = 0, pbtn = 0;
    for (;;) {
        vga_clear();
        vga_setcolor(VGA_LGREEN, VGA_BLACK);
        kputs("    __  ___                __         ____  _____\n");
        kputs("   / / / (_)________  ____/ /_____ _ / __ \\/ ___/\n");
        kputs("  / /_/ / / ___/ __ \\/ __  / __/ // // / / /\\__ \\ \n");
        kputs(" / __  / (__  ) /_/ / /_/ / /_/ ,< // /_/ /___/ / \n");
        kputs("/_/ /_/_/____/\\____/\\__,_/\\__/_/|_| \\____//____/  \n");
        vga_setcolor(VGA_DGREY, VGA_BLACK);
        kputs("\n");
        vga_setcolor(VGA_LGREY, VGA_BLACK);
        for (int i = 0; i < N; i++) {
            int y = 8 + i; uint8_t fg = (i==sel)?VGA_BLACK:VGA_LGREY, bg = (i==sel)?VGA_LGREY:VGA_BLACK;
            for (int x = 2; x <= 70; x++) vga_cell(x, y, ' ', fg, bg);
            vga_cell(3, y, (i==sel)?'>':' ', fg, bg);
            for (int k = 0; items[i][k]; k++) vga_cell(5+k, y, items[i][k], fg, bg);
        }
        vga_statusbar(" HOME   arrows or mouse   Enter / click open   Esc = terminal");
        vga_setcursor(0, 24);
        /* poll keyboard + mouse: hover highlights, click or Enter launches */
        int doquit = 0, launch = -1;
        for (;;) {
            char c = keyboard_trygetc();
            if      (c == 27)   { doquit = 1; break; }
            else if (c == 0x10) { sel = (sel + N - 1) % N; break; }
            else if (c == 0x0E) { sel = (sel + 1) % N; break; }
            else if (c == '\n') { launch = sel; break; }
            if (mouse_present()) {
                int mx, my, b; mouse_get(&mx, &my, &b);
                int hov = (my >= 8 && my < 8+N && mx >= 2 && mx <= 70) ? my - 8 : -1;
                if ((b & 1) && !(pbtn & 1)) { pbtn = b; if (hov >= 0) { sel = hov; launch = sel; } break; }
                pbtn = b;
                if (hov >= 0 && hov != sel) { sel = hov; break; }
            }
            __asm__ volatile("hlt");
        }
        if (doquit) break;                          /* Esc -> terminal */
        if (launch >= 0) {
            if      (launch == 0)  alexis_run();
            else if (launch == 1)  settings_run();
            else if (launch == 2)  explorer_run();
            else if (launch == 3)  app_editor();
            else if (launch == 4)  calc_app_run();
            else if (launch == 5)  menu_web();
            else if (launch == 6)  draw_run();
            else if (launch == 7)  clock_run();
            else if (launch == 8)  menu_tools();
            else if (launch == 9)  menu_games();
            else if (launch == 10) app_sysinfo();
            else if (launch == 11) { vga_clear(); cmd_man();
                                     vga_setcolor(VGA_DGREY, VGA_BLACK); kputs("\n  (any key)\n"); vga_setcolor(VGA_LGREY, VGA_BLACK);
                                     keyboard_getc(); }
            else if (launch == 12) break;            /* Terminal */
        }
    }
    vga_statusbar(" type 'menu' for Home    help  commands  man");
    vga_clear();
}

static void dispatch(char *line) {
    char *args = split(line);
    char *cmd = line;
    if (!*cmd) return;

    if      (!strcmp(cmd, "help"))    cmd_help();
    else if (!strcmp(cmd, "commands"))cmd_commands();
    else if (!strcmp(cmd, "clear"))   vga_clear();
    else if (!strcmp(cmd, "cls"))     vga_clear();
    else if (!strcmp(cmd, "cpuid"))   cmd_cpuid();
    else if (!strcmp(cmd, "calc"))    cmd_calc(args);
    else if (!strcmp(cmd, "cal"))     cmd_cal();
    else if (!strcmp(cmd, "ascii"))   cmd_ascii();
    else if (!strcmp(cmd, "rand"))    cmd_rand();
    else if (!strcmp(cmd, "poweroff"))screen_message("Shutting down HisokaOS...", 2);
    else if (!strcmp(cmd, "shutdown"))screen_message("Shutting down HisokaOS...", 2);
    else if (!strcmp(cmd, "restart")) screen_message("Restarting HisokaOS...", 1);
    else if (!strcmp(cmd, "bootscreen"))    { splash_boot(); vga_clear(); }
    else if (!strcmp(cmd, "shutdownscreen")){ screen_message("Shutting down HisokaOS...", 0); vga_clear(); }
    else if (!strcmp(cmd, "restartscreen")) { screen_message("Restarting HisokaOS...", 0); vga_clear(); }
    else if (!strcmp(cmd, "panicscreen"))   { panic_sim(); vga_clear(); }
    else if (!strcmp(cmd, "panictest"))     kernel_panic("test panic (panictest command)");
    else if (!strcmp(cmd, "update"))        cmd_update(args);
    else if (!strcmp(cmd, "upgrade"))       cmd_update(args);
    else if (!strcmp(cmd, "changelog"))     cmd_changelog();
    else if (!strcmp(cmd, "whatsnew"))      cmd_changelog();
    else if (!strcmp(cmd, "setup"))         setup_wizard();
    else if (!strcmp(cmd, "reset"))         cmd_reset(args);
    else if (!strcmp(cmd, "factory-reset")) cmd_reset(args);
    else if (!strcmp(cmd, "agent"))         agentinfo_describe(1);
    else if (!strcmp(cmd, "agentinfo"))     agentinfo_describe(1);
    else if (!strcmp(cmd, "whatami"))       agentinfo_describe(0);
    else if (!strcmp(cmd, "more"))          cmd_more(P(args));
    else if (!strcmp(cmd, "less"))          cmd_more(P(args));
    else if (!strcmp(cmd, "open"))          cmd_open(args);
    else if (!strcmp(cmd, "download"))      cmd_download(args);
    else if (!strcmp(cmd, "wget"))          cmd_download(args);
    else if (!strcmp(cmd, "which"))         cmd_which(args);
    else if (!strcmp(cmd, "realpath"))      kprintf("%s\n", P(args));
    else if (!strcmp(cmd, "id"))            cmd_id();
    else if (!strcmp(cmd, "true"))          { /* exit 0, no output */ }
    else if (!strcmp(cmd, "false"))         { /* exit 1, no output */ }
    else if (!strcmp(cmd, "arch"))          kputs("i386\n");
    else if (!strcmp(cmd, "nproc"))         kputs("1\n");
    else if (!strcmp(cmd, "printf"))        cmd_printf(args);
    else if (!strcmp(cmd, "expr"))          cmd_calc(args);
    else if (!strcmp(cmd, "base64"))        cmd_base64(args);
    else if (!strcmp(cmd, "sum"))           cmd_sum(P(args));
    else if (!strcmp(cmd, "strings"))       cmd_strings(P(args));
    else if (!strcmp(cmd, "od"))            cmd_od(P(args));
    else if (!strcmp(cmd, "fold"))          cmd_fold(P(args));
    else if (!strcmp(cmd, "cksum"))         cmd_cksum(P(args));
    else if (!strcmp(cmd, "cut"))           cmd_cut(args);
    else if (!strcmp(cmd, "groups"))        kputs(is_admin() ? "admin wheel users\n" : "users\n");
    else if (!strcmp(cmd, "tty"))           kputs("/dev/tty\n");
    else if (!strcmp(cmd, "printenv"))      cmd_env();
    else if (!strcmp(cmd, "dir") || !strcmp(cmd, "vdir")) cmd_ls(P(args));
    else if (!strcmp(cmd, "egrep") || !strcmp(cmd, "fgrep")) cmd_grep(args);
    else if (!strcmp(cmd, "type"))          cmd_which(args);
    else if (!strcmp(cmd, "img"))           imgview_run(P(args));
    else if (!strcmp(cmd, "view"))          imgview_run(P(args));
    else if (!strcmp(cmd, "image"))         imgview_run(P(args));
    else if (!strcmp(cmd, "fcv"))           cmd_fcv(args);
    else if (!strcmp(cmd, "convert"))       cmd_fcv(args);
    else if (!strcmp(cmd, "alexis"))        alexis_run();
    else if (!strcmp(cmd, "ai"))            alexis_run();
    else if (!strcmp(cmd, "ask"))           alexis_run();
    else if (!strcmp(cmd, "todo"))          todo_run();
    else if (!strcmp(cmd, "monitor"))       monitor_run();
    else if (!strcmp(cmd, "top"))           monitor_run();
    else if (!strcmp(cmd, "units"))         units_run();
    else if (!strcmp(cmd, "stopwatch"))     stopwatch_run();
    else if (!strcmp(cmd, "base"))          baseconv_run();
    else if (!strcmp(cmd, "passgen"))       passgen_run();
    else if (!strcmp(cmd, "calendar"))      calendar_run();
    else if (!strcmp(cmd, "contacts"))      contacts_run();
    else if (!strcmp(cmd, "bookmarks"))     bookmarks_run();
    else if (!strcmp(cmd, "timer"))         timer_run();
    else if (!strcmp(cmd, "counter"))       counter_run();
    else if (!strcmp(cmd, "expenses"))      expenses_run();
    else if (!strcmp(cmd, "media"))         media_run();
    else if (!strcmp(cmd, "player"))        media_run();
    else if (!strcmp(cmd, "worldclock"))    worldclock_run();
    else if (!strcmp(cmd, "nettools"))      nettools_run();
    else if (!strcmp(cmd, "diskusage"))     diskusage_run();
    else if (!strcmp(cmd, "kanban"))        kanban_run();
    else if (!strcmp(cmd, "notes"))         notes_run();
    else if (!strcmp(cmd, "finance"))       finance_run();
    else if (!strcmp(cmd, "tip"))           tip_run();
    else if (!strcmp(cmd, "bmi"))           bmi_run();
    else if (!strcmp(cmd, "habits"))        habits_run();
    else if (!strcmp(cmd, "datecalc"))      datecalc_run();
    else if (!strcmp(cmd, "roman"))         roman_run();
    else if (!strcmp(cmd, "cipher"))        cipher_run();
    else if (!strcmp(cmd, "morse"))         morse_run();
    else if (!strcmp(cmd, "primes"))        primes_run();
    else if (!strcmp(cmd, "percent"))       percent_run();
    else if (!strcmp(cmd, "textstats"))     textstats_run();
    else if (!strcmp(cmd, "num2words"))     num2words_run();
    else if (!strcmp(cmd, "pomodoro"))      pomodoro_run();
    else if (!strcmp(cmd, "timestable"))    timestable_run();
    else if (!strcmp(cmd, "gcd"))           gcdlcm_run();
    else if (!strcmp(cmd, "factorial"))     factorial_run();
    else if (!strcmp(cmd, "fibonacci"))     fibonacci_run();
    else if (!strcmp(cmd, "reverse"))       reverse_run();
    else if (!strcmp(cmd, "lorem"))         lorem_run();
    else if (!strcmp(cmd, "eightball"))     eightball_run();
    else if (!strcmp(cmd, "discount"))      discount_run();
    else if (!strcmp(cmd, "currency"))      currency_run();
    else if (!strcmp(cmd, "savings"))       savings_run();
    else if (!strcmp(cmd, "colorpick") || !strcmp(cmd, "color")) colorpick_run();
    else if (!strcmp(cmd, "binclock"))      binclock_run();
    else if (!strcmp(cmd, "piglatin"))      piglatin_run();
    else if (!strcmp(cmd, "water"))         water_run();
    else if (!strcmp(cmd, "leet"))          leet_run();
    else if (!strcmp(cmd, "mouse"))         cmd_mousetest();
    else if (!strcmp(cmd, "mousetest"))     cmd_mousetest();
    else if (!strcmp(cmd, "beep"))          speaker_beep(880, 150);
    else if (!strcmp(cmd, "piano"))         piano_run();
    else if (!strcmp(cmd, "backup") || !strcmp(cmd, "backups")) backup_run();
    else if (!strcmp(cmd, "forum"))         forum_run();
    else if (!strcmp(cmd, "tools"))         menu_tools();
    else if (!strcmp(cmd, "cp"))      cmd_cp(args);
    else if (!strcmp(cmd, "mv"))      cmd_mv(args);
    else if (!strcmp(cmd, "wc"))      cmd_wc(P(args));
    else if (!strcmp(cmd, "mem"))     cmd_mem();
    else if (!strcmp(cmd, "vm"))      cmd_vm();
    else if (!strcmp(cmd, "pgfault")) { kputs("touching unmapped memory at 0xF0000000 ...\n");
                                        volatile uint32_t *bad = (uint32_t *)0xF0000000u;
                                        uint32_t v = *bad;   /* -> #PF, handler prints CR2 and halts */
                                        kprintf("read %u (unexpected: should have faulted)\n", v); }
    else if (!strcmp(cmd, "sec"))     cmd_sec();
    else if (!strcmp(cmd, "lspci"))   cmd_lspci();
    else if (!strcmp(cmd, "net"))     cmd_net();
    else if (!strcmp(cmd, "arp"))     cmd_arp(args);
    else if (!strcmp(cmd, "ping"))    cmd_ping(args);
    else if (!strcmp(cmd, "dns"))     cmd_dns(args);
    else if (!strcmp(cmd, "nslookup"))cmd_dns(args);
    else if (!strcmp(cmd, "fetch"))   cmd_fetch(args);
    else if (!strcmp(cmd, "curl"))    cmd_fetch(args);
    else if (!strcmp(cmd, "browse"))  browser_run(args);
    else if (!strcmp(cmd, "chromium"))browser_run(args);
    else if (!strcmp(cmd, "snake"))   snake_run();
    else if (!strcmp(cmd, "2048"))    g2048_run();
    else if (!strcmp(cmd, "tetris"))  tetris_run();
    else if (!strcmp(cmd, "ttt"))     ttt_run();
    else if (!strcmp(cmd, "calculator")) calc_app_run();
    else if (!strcmp(cmd, "date"))    cmd_date();
    else if (!strcmp(cmd, "uname"))   kputs("HisokaOS 0.2 i386\n");
    else if (!strcmp(cmd, "about"))   kputs("HisokaOS - a 32-bit x86 operating system.\n");
    else if (!strcmp(cmd, "uptime"))  { uint32_t t = pit_ticks(); kprintf("up %u.%u s\n", t/100, (t%100)/10); }
    else if (!strcmp(cmd, "reboot"))  screen_message("Restarting HisokaOS...", 1);
    else if (!strcmp(cmd, "panic"))   { volatile int z = 0; kprintf("%d", 1/z); }
    else if (!strcmp(cmd, "echo"))    { kputs(args); kputs("\n"); }
    else if (!strcmp(cmd, "edit"))    build_run(P(args));
    else if (!strcmp(cmd, "build"))   build_run(P(args));
    else if (!strcmp(cmd, "code"))    build_run(P(args));
    else if (!strcmp(cmd, "grep"))    cmd_grep(args);
    else if (!strcmp(cmd, "find"))    cmd_find(args);
    else if (!strcmp(cmd, "head"))    cmd_head(P(args));
    else if (!strcmp(cmd, "tail"))    cmd_tail(P(args));
    else if (!strcmp(cmd, "rev"))     cmd_rev(args);
    else if (!strcmp(cmd, "upper"))   cmd_case(args, 1);
    else if (!strcmp(cmd, "lower"))   cmd_case(args, 0);
    else if (!strcmp(cmd, "seq"))     cmd_seq(args);
    else if (!strcmp(cmd, "ls"))      cmd_ls(P(args));
    else if (!strcmp(cmd, "cd"))      cmd_cd(args);
    else if (!strcmp(cmd, "pwd"))     cmd_pwd();
    else if (!strcmp(cmd, "mkdir"))   cmd_mkdir(args);
    else if (!strcmp(cmd, "rmdir"))   cmd_rmdir(P(args));
    else if (!strcmp(cmd, "tree"))    cmd_tree(args);
    else if (!strcmp(cmd, "cat"))     cmd_cat(P(args));
    else if (!strcmp(cmd, "touch"))   { const char *p = P(args); kprintf(fs_create(p) == 0 ? "created %s\n" : "touch: %s exists/full\n", p); }
    else if (!strcmp(cmd, "rm"))      cmd_rm_path(P(args));
    else if (!strcmp(cmd, "stat"))    { fs_file_t *f = fs_find(P(args)); if (f) kprintf("%s: %u bytes%s\n", f->name, (uint32_t)f->len, f->is_dir ? "  (directory)" : ""); else kprintf("stat: %s not found\n", P(args)); }
    else if (!strcmp(cmd, "write"))   { char *txt = split(args); const char *p = P(args); fs_write(p, txt, strlen(txt)); kprintf("wrote %u bytes to %s\n", (uint32_t)strlen(txt), p); }
    else if (!strcmp(cmd, "append"))  { char *txt = split(args); const char *p = P(args); fs_append(p, txt, strlen(txt)); fs_append(p, "\n", 1); kprintf("appended to %s\n", p); }
    else if (!strcmp(cmd, "history")) { for (int i = 0; i < hist_n; i++) kprintf("  %u  %s\n", i + 1, hist[i]); }
    else if (!strcmp(cmd, "man"))      cmd_man();
    else if (!strcmp(cmd, "motd"))     cmd_motd();
    else if (!strcmp(cmd, "keys"))     cmd_keys();
    else if (!strcmp(cmd, "multitask")) cmd_multitask();
    else if (!strcmp(cmd, "threads"))  cmd_multitask();
    else if (!strcmp(cmd, "ps"))       cmd_ps();
    else if (!strcmp(cmd, "gfx") || !strcmp(cmd, "graphics")) {
        if (!gfx_available()) kputs("gfx: no graphics adapter found\n");
        else { kputs("switching to graphics mode... (press any key to return to text)\n");
               gfx_demo(); keyboard_getc(); gfx_exit(); }
    }
    else if (!strcmp(cmd, "log") || !strcmp(cmd, "logs")) {
        fs_file_t *lf = fs_find("/var/log/system.log");
        static char lb[16384]; size_t ln = 0;
        if (lf && lf->data) { ln = lf->len < sizeof(lb)-1 ? lf->len : sizeof(lb)-1; memcpy(lb, lf->data, ln); }
        lb[ln] = 0;
        ui_scrollbox_view("System Log - everything the OS does", lb[0] ? lb : "(log is empty)");
    }
    else if (!strcmp(cmd, "dmesg"))    kputs(klog_buffer());
    else if (!strcmp(cmd, "explorer")) explorer_run();
    else if (!strcmp(cmd, "files"))    explorer_run();
    else if (!strcmp(cmd, "file"))     cmd_file(P(args));
    else if (!strcmp(cmd, "run"))      cmd_run(P(args));
    else if (!strcmp(cmd, "sh"))       cmd_run(P(args));
    else if (!strcmp(cmd, "apps"))     cmd_apps();
    else if (!strcmp(cmd, "menu"))     menu_run();
    else if (!strcmp(cmd, "home"))     menu_run();
    else if (!strcmp(cmd, "start"))    menu_run();
    else if (!strcmp(cmd, "settings")) settings_run();
    else if (!strcmp(cmd, "control"))  settings_run();
    else if (!strcmp(cmd, "draw"))     draw_run();
    else if (!strcmp(cmd, "paint"))    draw_run();
    else if (!strcmp(cmd, "clock"))    clock_run();
    else if (!strcmp(cmd, "pwd"))      cmd_pwd();
    else if (!strcmp(cmd, "whoami"))   cmd_whoami();
    else if (!strcmp(cmd, "hostname")) cmd_hostname();
    else if (!strcmp(cmd, "env"))      cmd_env();
    else if (!strcmp(cmd, "yes"))      cmd_yes(args);
    else if (!strcmp(cmd, "factor"))   cmd_factor(args);
    else if (!strcmp(cmd, "basename")) cmd_basename(args);
    else if (!strcmp(cmd, "dirname"))  cmd_dirname(args);
    else if (!strcmp(cmd, "tac"))      cmd_tac(P(args));
    else if (!strcmp(cmd, "nl"))       cmd_nl(P(args));
    else if (!strcmp(cmd, "sort"))     cmd_sort(P(args));
    else if (!strcmp(cmd, "uniq"))     cmd_uniq(P(args));
    else if (!strcmp(cmd, "hexdump"))  cmd_hexdump(P(args));
    else if (!strcmp(cmd, "xxd"))      cmd_hexdump(P(args));
    else if (!strcmp(cmd, "du"))       cmd_du();
    else if (!strcmp(cmd, "df"))       cmd_df();
    else if (!strcmp(cmd, "sync"))     { if (!ata_present()) kputs("sync: no disk attached\n");
                                         else { int r = persist_save(); kprintf(r == 0 ? "synced: %u files saved to disk\n" : "sync: write error %d\n", r == 0 ? (uint32_t)fs_count() : (uint32_t)r); } }
    else if (!strcmp(cmd, "free"))     cmd_mem();
    else if (!strcmp(cmd, "sleep"))    cmd_sleep(args);
    else if (!strcmp(cmd, "flip"))     cmd_flip();
    else if (!strcmp(cmd, "roll"))     cmd_roll(args);
    else if (!strcmp(cmd, "dice"))     cmd_roll(args);
    else if (!strcmp(cmd, "len"))      cmd_len(args);
    else if (!strcmp(cmd, "hex"))      cmd_hex(args);
    else if (!strcmp(cmd, "bin"))      cmd_bin(args);
    else if (!strcmp(cmd, "ord"))      cmd_ord(args);
    else if (!strcmp(cmd, "chr"))      cmd_chr(args);
    else if (!strcmp(cmd, "box"))      cmd_box(args);
    else if (!strcmp(cmd, "cowsay"))   cmd_cowsay(args);
    else if (!strcmp(cmd, "repeat"))   cmd_repeat(args);
    else kprintf("unknown command: %s  (try 'help')\n", cmd);
}

void shell_run(void) {
    char line[LINE], raw[LINE];
    seed_files();                         /* starter files + the system directory tree */
    write_manual();                       /* drop manual.txt into the file explorer */
    klog_fs_ready();                      /* mirror the boot log to /var/log/system.log */
    app_seed();                           /* apps appear as files in /Applications */
    explorer_set_opener(open_path);       /* Enter in Files launches apps / runs scripts */
    explorer_set_terminal(set_current_file); /* 't' in Files sends a file to the terminal */
    fs_set_write_hook(on_fs_write);       /* /sys control files react live; writes are logged */
    apply_saved_theme();                  /* restore the user's accent color from config */
    if (!fs_find("/etc/.setup-done")) {   /* first boot -> friendly setup wizard */
        klog("setup: first boot, running setup wizard");
        setup_wizard();
        apply_saved_theme();
    }
    login_gate();                         /* if a passcode was set, require it to unlock */
    update_prompt();                      /* if an update is available: Yes / Don't-ask / Later */
    klog("session: shell ready, home screen shown");
    menu_run();                           /* boot straight into the friendly Home screen */
    vga_setcolor(VGA_WHITE, VGA_BLACK);
    kputs("\n");
    for (;;) {
        prompt();
        readline(raw);
        char *a = raw; while (*a == ' ') a++;
        if (!*a) continue;
        if (hist_n < HIST) strcpy(hist[hist_n++], a);   /* record history */
        klog(a);                                        /* log every command */
        strcpy(line, a);                                /* dispatch mutates line */
        dispatch(line);
    }
}
