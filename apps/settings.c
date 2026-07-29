/* settings.c - Control Center. Change the desktop accent color (recolors the
 * title/status bars and borders live) and view system information. */
#include "settings.h"
#include "vga.h"
#include "keyboard.h"
#include "pmm.h"
#include "pit.h"
#include "ramfs.h"
#include "rtc.h"
#include "printf.h"
#include "klog.h"
#include "ui.h"
#include "types.h"

static int s_u(char *b, int o, uint32_t v) { char t[12]; int m=0; if(!v)t[m++]='0'; while(v){t[m++]=(char)('0'+v%10);v/=10;} while(m)b[o++]=t[--m]; return o; }
static int s_s(char *b, int o, const char *s) { while(*s)b[o++]=*s++; return o; }

/* delete all files directly inside /tmp; returns how many were removed */
static int clear_temp(void) {
    int n = 0;
    for (int i = 0; i < FS_MAX_FILES; i++) {
        fs_file_t *f = fs_at(i);
        if (!f || !f->used || f->is_dir) continue;
        const char *nm = f->name;
        if (nm[0]=='/'&&nm[1]=='t'&&nm[2]=='m'&&nm[3]=='p'&&nm[4]=='/') {
            int deeper = 0; for (const char *p = nm + 5; *p; p++) if (*p == '/') deeper = 1;
            if (!deeper) { fs_delete(nm); n++; }
        }
    }
    if (n) klog("settings: cleared temp files");
    return n;
}

void settings_run(void) {
    static const uint8_t cols[]  = { VGA_BLUE, VGA_GREEN, VGA_CYAN, VGA_RED, VGA_MAGENTA, VGA_DGREY, VGA_BROWN };
    static const char   *names[] = { "Blue", "Green", "Cyan", "Red", "Magenta", "Grey", "Brown" };
    const int ncol = 7;
    int ci = 0, quit = 0;
    char msg[40] = {0};

    while (!quit) {
        vga_clear();
        ui_panel(2, 1, 76, 22, "Control Center", VGA_LCYAN, VGA_BLACK);
        char l[64]; int o;
        ui_text(5, 3, "Accent", VGA_LGREY, VGA_BLACK);
        ui_text(16, 3, "<", VGA_DGREY, VGA_BLACK); ui_text(18, 3, names[ci], VGA_LCYAN, VGA_BLACK);
        { int nl = 0; while (names[ci][nl]) nl++; ui_text(19+nl, 3, ">", VGA_DGREY, VGA_BLACK); }
        ui_text(5, 5, "System", VGA_YELLOW, VGA_BLACK);
        ui_text(7, 6, "Device  : hisoka      OS: HisokaOS 0.2 (i386)", VGA_LGREY, VGA_BLACK);
        uint32_t tot = pmm_total_frames(), fr = pmm_free_frames();
        o=s_s(l,0,"Memory  : "); o=s_u(l,o,(tot-fr)*4/1024); o=s_s(l,o," / "); o=s_u(l,o,tot*4/1024); o=s_s(l,o," MiB used"); l[o]=0; ui_text(7,7,l,VGA_LGREY,VGA_BLACK);
        o=s_s(l,0,"Files   : "); o=s_u(l,o,(uint32_t)fs_count()); l[o]=0; ui_text(7,8,l,VGA_LGREY,VGA_BLACK);
        o=s_s(l,0,"Uptime  : "); o=s_u(l,o,pit_ticks()/100); o=s_s(l,o," s"); l[o]=0; ui_text(7,9,l,VGA_LGREY,VGA_BLACK);
        rtc_time_t t; rtc_now(&t);
        o=s_s(l,0,"Clock   : "); o=s_u(l,o,t.year); l[o++]='-'; o=s_u(l,o,t.month); l[o++]='-'; o=s_u(l,o,t.day); l[o++]=' '; if(t.hour<10)l[o++]='0'; o=s_u(l,o,t.hour); l[o++]=':'; if(t.min<10)l[o++]='0'; o=s_u(l,o,t.min); l[o]=0; ui_text(7,10,l,VGA_LGREY,VGA_BLACK);
        ui_text(7, 11, "Network : 10.0.2.15", VGA_LGREY, VGA_BLACK);
        ui_text(5, 13, "Maintenance", VGA_YELLOW, VGA_BLACK);
        int tmpn = 0;
        for (int i = 0; i < FS_MAX_FILES; i++) { fs_file_t *tf = fs_at(i); if (tf && tf->used && !tf->is_dir && tf->name[0]=='/'&&tf->name[1]=='t'&&tf->name[2]=='m'&&tf->name[3]=='p'&&tf->name[4]=='/') { int d=0; for (const char*p=tf->name+5;*p;p++) if(*p=='/')d=1; if(!d) tmpn++; } }
        o=s_s(l,0,"Temp    : "); o=s_u(l,o,(uint32_t)tmpn); o=s_s(l,o," files in /tmp"); l[o]=0; ui_text(7,14,l,VGA_LGREY,VGA_BLACK);
        if (msg[0]) ui_text(7, 15, msg, VGA_LGREEN, VGA_BLACK);

        vga_statusbar(" SETTINGS   < > theme   c clear temp   q quit");
        vga_setcursor(0, 24);

        char c = keyboard_getc();
        if      (c == 'q' || c == 'Q' || c == 27) quit = 1;
        else if (c == 0x02) { ci = (ci + ncol - 1) % ncol; vga_set_theme(VGA_WHITE, cols[ci]); }
        else if (c == 0x06 || c == '\n') { ci = (ci + 1) % ncol; vga_set_theme(VGA_WHITE, cols[ci]); }
        else if (c == 'c' || c == 'C') { int n = clear_temp(); int o = 0; const char *a = "Cleared "; while (*a) msg[o++]=*a++; int v=n; char tmp[8]; int tn=0; if(!v)tmp[tn++]='0'; while(v){tmp[tn++]=(char)('0'+v%10);v/=10;} while(tn)msg[o++]=tmp[--tn]; const char *b=" temp file(s)."; while(*b)msg[o++]=*b++; msg[o]=0; }
    }
    vga_statusbar("help  commands  man       HisokaOS 0.2  i386");
    vga_clear();
    vga_setcolor(VGA_LCYAN, VGA_BLACK); kputs("Control Center closed.\n");
    vga_setcolor(VGA_LGREY, VGA_BLACK);
}
