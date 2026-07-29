/* procfs.c - a live /proc, the way a real OS exposes its guts as files. Every time
 * /proc is read (cat or opened in Files), these files are regenerated with the
 * current memory, uptime, network and system state. Real data, browsable as text. */
#include "procfs.h"
#include "ramfs.h"
#include "pmm.h"
#include "heap.h"
#include "pit.h"
#include "net.h"
#include "rtl8139.h"
#include "version.h"
#include "string.h"

static char PB[4096];
static int  PBO;
static void p_reset(void) { PBO = 0; }
static void p_s(const char *s) { while (*s && PBO < 4094) PB[PBO++] = *s++; }
static void p_u(uint32_t v) { char t[12]; int n = 0; if (!v) t[n++] = '0'; while (v) { t[n++] = (char)('0'+v%10); v/=10; } while (n) PB[PBO++] = t[--n]; }
static void p_ip(const uint8_t *ip) { p_u(ip[0]); p_s("."); p_u(ip[1]); p_s("."); p_u(ip[2]); p_s("."); p_u(ip[3]); }
static void p_write(const char *path) { PB[PBO] = 0; fs_write(path, PB, (size_t)PBO); }

void procfs_refresh(void) {
    fs_mkdir("/proc");
    uint32_t tot = pmm_total_frames(), fr = pmm_free_frames();
    uint32_t up = pit_ticks() / 100;

    p_reset();
    p_s("MemTotal:   "); p_u(tot*4); p_s(" kB\n");
    p_s("MemFree:    "); p_u(fr*4);  p_s(" kB\n");
    p_s("MemUsed:    "); p_u((tot-fr)*4); p_s(" kB\n");
    p_s("Frames:     "); p_u(tot-fr); p_s(" / "); p_u(tot); p_s(" (4 KiB each)\n");
    p_s("HeapUsed:   "); p_u((uint32_t)heap_used()); p_s(" bytes\n");
    p_write("/proc/meminfo");

    p_reset();
    p_s("processor   : 0\n");
    p_s("arch        : " OS_ARCH "\n");
    p_s("mode        : 32-bit protected mode\n");
    p_s("features    : gdt idt paging(PSE) scheduler\n");
    p_s("fpu         : none (software only)\n");
    p_write("/proc/cpuinfo");

    p_reset();
    p_u(up); p_s(" seconds\n");
    p_write("/proc/uptime");

    p_reset();
    p_s(OS_NAME " " OS_VERSION " (" OS_ARCH ") revision "); p_u(SYS_REVISION);
    p_s("\nfrom-scratch C kernel, Multiboot1, runs under QEMU\n");
    p_write("/proc/version");

    p_reset();
    p_s("filesystem  : ramfs (in-memory, persisted to ATA disk)\n");
    p_s("files        : "); p_u((uint32_t)fs_count()); p_s("\n");
    p_s("mounts       : / (ramfs)   disk0 (ata, sector 0 blob)\n");
    p_write("/proc/mounts");

    p_reset();
    if (rtl8139_present()) {
        const uint8_t *ip = net_my_ip(), *gw = net_gw_ip();
        p_s("device   : rtl8139 (PCI 10ec:8139)\n");
        p_s("ip       : "); p_ip(ip); p_s("\n");
        p_s("gateway  : "); p_ip(gw); p_s("\n");
        p_s("dns      : 10.0.2.3\n");
        p_s("stack    : Ethernet/ARP/IPv4/ICMP/UDP/DNS/TCP/HTTP (from scratch)\n");
    } else {
        p_s("device   : none (no NIC detected)\n");
    }
    p_write("/proc/net");

    p_reset();
    p_s("uptime_ticks "); p_u(pit_ticks()); p_s("\n");
    p_s("uptime_sec   "); p_u(up); p_s("\n");
    p_s("mem_frames   "); p_u(tot); p_s("\n");
    p_s("mem_free     "); p_u(fr); p_s("\n");
    p_write("/proc/stat");

    p_reset();
    p_s("irq  device\n");
    p_s("  0  PIT (timer, 100 Hz)\n");
    p_s("  1  PS/2 keyboard\n");
    p_s(" 12  PS/2 mouse (planned)\n");
    p_s(" 11  RTL8139 NIC\n");
    p_write("/proc/interrupts");

    p_reset();
    p_s("multiboot kernel loaded by QEMU -kernel; console=vga + serial(COM1)\n");
    p_write("/proc/cmdline");
}
