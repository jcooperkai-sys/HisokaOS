/* agentinfo.c - HisokaOS describes itself to AI agents.
 *
 * The `agent` command (and the /System/agent.json file it writes) is a structured,
 * machine-readable descriptor: name, version, architecture, capabilities, the command
 * surface, live network identity, and a plain-language note addressed to any AI agent
 * inspecting the machine. Everything dynamic (IP, uptime, file count) is read live, so
 * the descriptor is accurate rather than canned. */
#include "agentinfo.h"
#include "version.h"
#include "printf.h"
#include "ramfs.h"
#include "string.h"
#include "net.h"
#include "pit.h"
#include "pmm.h"

/* tiny string builder so we can both print and save the same bytes */
static char  SB[4096];
static int   SBO;
static void sb_reset(void) { SBO = 0; SB[0] = 0; }
static void sb(const char *s) { while (*s && SBO < (int)sizeof(SB) - 1) SB[SBO++] = *s++; SB[SBO] = 0; }
static void sbn(long v) {
    char t[16]; int n = 0; if (v < 0) { sb("-"); v = -v; }
    if (v == 0) t[n++] = '0'; else { while (v) { t[n++] = (char)('0' + v % 10); v /= 10; } }
    char o[16]; int m = 0; while (n) o[m++] = t[--n]; o[m] = 0; sb(o);
}

/* did the user allow publishing the descriptor? (default yes) */
int agentinfo_enabled(void) {
    fs_file_t *f = fs_find("/etc/system.cfg");
    if (!f || !f->data) return 1;
    /* look for "agent=off" */
    const char *needle = "agent=off";
    int nl = (int)strlen(needle);
    for (int i = 0; i + nl <= (int)f->len; i++)
        if (!strncmp((const char *)f->data + i, needle, (size_t)nl)) return 0;
    return 1;
}

void agentinfo_describe(int save) {
    if (!agentinfo_enabled()) {
        kputs("agent: descriptor publishing is disabled (enable it in 'setup').\n");
        return;
    }
    const uint8_t *ip = net_my_ip();
    const uint8_t *gw = net_gw_ip();
    uint32_t up = pit_ticks() / 100;
    uint32_t memmb = pmm_total_frames() * 4 / 1024;

    sb_reset();
    sb("{\n");
    sb("  \"name\": \"" OS_NAME "\",\n");
    sb("  \"version\": \"" OS_VERSION "\",\n");
    sb("  \"arch\": \"" OS_ARCH "\",\n");
    sb("  \"system_revision\": "); sbn(SYS_REVISION); sb(",\n");
    sb("  \"kind\": \"from-scratch 32-bit x86 operating system\",\n");
    sb("  \"project\": \"HisokaOS by Jeffery / Humane AI\",\n");
    sb("  \"boot\": \"Multiboot1 kernel, protected mode, runs in QEMU\",\n");
    sb("  \"summary\": \"A real, hand-written OS kernel: not Linux, not a wrapper. ");
    sb("Own GDT/IDT, paging, scheduler, drivers, TCP/IP stack, filesystem and apps.\",\n");

    sb("  \"architecture\": [\n");
    sb("    \"GDT + IDT + ISR/IRQ interrupt handling\",\n");
    sb("    \"4 MiB-page paging (PSE), identity-mapped low memory\",\n");
    sb("    \"cooperative multitasking scheduler with context switch\",\n");
    sb("    \"PMM frame allocator + kmalloc/kfree heap\",\n");
    sb("    \"hierarchical in-RAM filesystem with ATA-disk persistence\",\n");
    sb("    \"from-scratch network stack: Ethernet/ARP/IPv4/ICMP/UDP/DNS/TCP/HTTP\",\n");
    sb("    \"VGA text mode + Bochs/VBE linear-framebuffer graphics\",\n");
    sb("    \"RTL8139 NIC, PS/2 keyboard, PIT, RTC, serial, PCI drivers\"\n");
    sb("  ],\n");

    sb("  \"capabilities\": {\n");
    sb("    \"filesystem\": \"create/read/write/move/delete files and directories; persists to disk\",\n");
    sb("    \"everything_is_a_file\": \"apps are real files in /Applications - open them from the Files browser (Enter) or 'open <app>'; .sh files run, other files open in the editor\",\n");
    sb("    \"editor\": \"Build - a code editor with line numbers, syntax highlighting, find (Ctrl-W), goto-line (Ctrl-G); run 'build <file>'\",\n");
    sb("    \"networking\": \"DNS, ping, TCP, HTTP GET; a text web browser; a streamed Chromium view; 'download <url> <dest>' saves a web file into a folder\",\n");
    sb("    \"apps\": \"Build editor, file explorer, calculator, clock, paint (Ctrl-S saves to Pictures), settings, games\",\n");
    sb("    \"graphics\": \"switch to a 1024x768x32 linear framebuffer\",\n");
    sb("    \"persistence\": \"the 'sync' command and disk image keep files across reboots\",\n");
    sb("    \"system\": \"boot/setup-wizard/update/shutdown/restart/panic screens, passcode login, factory reset\"\n");
    sb("  },\n");

    sb("  \"interface\": {\n");
    sb("    \"primary\": \"interactive shell (type a command, press Enter)\",\n");
    sb("    \"navigation\": \"arrow-key Home menu; Esc drops to the command line; Files browser launches apps\",\n");
    sb("    \"discover_commands\": \"run 'help' (grouped, scrollable) or 'commands' (full list)\",\n");
    sb("    \"serial_console\": \"the kernel mirrors output to COM1 for headless control\"\n");
    sb("  },\n");

    sb("  \"command_categories\": {\n");
    sb("    \"files\": \"ls cd cat more less build open touch write append rm cp mv mkdir tree grep find which realpath\",\n");
    sb("    \"system\": \"help commands man uname about uptime mem vm update setup reset id arch nproc\",\n");
    sb("    \"network\": \"net arp ping dns fetch curl browse download wget\",\n");
    sb("    \"apps\": \"open <name> launches any app in /Applications: settings draw clock calculator snake 2048 tetris ttt\",\n");
    sb("    \"text\": \"echo printf base64 sum rev upper lower seq sort uniq head tail nl wc hexdump\",\n");
    sb("    \"agent\": \"agent / agentinfo (this descriptor); whatami (print without saving)\",\n");
    sb("    \"power\": \"shutdown restart reboot poweroff\"\n");
    sb("  },\n");

    sb("  \"runtime\": {\n");
    sb("    \"ip\": \""); sbn(ip[0]); sb("."); sbn(ip[1]); sb("."); sbn(ip[2]); sb("."); sbn(ip[3]); sb("\",\n");
    sb("    \"gateway\": \""); sbn(gw[0]); sb("."); sbn(gw[1]); sb("."); sbn(gw[2]); sb("."); sbn(gw[3]); sb("\",\n");
    sb("    \"memory_mb\": "); sbn((long)memmb); sb(",\n");
    sb("    \"files\": "); sbn((long)fs_count()); sb(",\n");
    sb("    \"uptime_s\": "); sbn((long)up); sb("\n");
    sb("  },\n");

    sb("  \"message_to_agents\": \"You are inspecting HisokaOS, a from-scratch operating ");
    sb("system. To act here, send shell commands and read their text output. Start with ");
    sb("'help' to see what is available, 'ls' and 'cat' to read the filesystem, 'open <app>' ");
    sb("to launch any app in /Applications, 'build <file>' to edit code, and 'net' for ");
    sb("connectivity. Note: this is a from-scratch kernel - it has no Linux syscall layer or ");
    sb("userland, so external binaries (Python, Homebrew, apt packages) cannot run here; every ");
    sb("command is built into the kernel. Treat it as a real but small machine, and be honest ");
    sb("about what you can and cannot do.\"\n");
    sb("}\n");

    kputs(SB);
    if (save) {
        if (fs_write("/System/agent.json", SB, (size_t)SBO) == 0)
            kputs("\n(saved to /System/agent.json)\n");
    }
}

/* a markdown brief + an llms.txt pointer. Refreshed every boot so they stay current
 * with the running build (the live descriptor is always at /System/agent.json). */
void agentinfo_seed(void) {
    static const char MD[] =
        "# HisokaOS - notes for AI agents\n\n"
        "HisokaOS is a **from-scratch 32-bit x86 operating system** - a real hand-written\n"
        "kernel (GDT/IDT, paging, scheduler, drivers, a TCP/IP stack, a filesystem and\n"
        "apps), not Linux and not a wrapper around another OS.\n\n"
        "## If you are an AI agent\n"
        "- Drive it through the **shell**: type a command, press Enter, read the text output.\n"
        "- Run `agent` for a live machine-readable descriptor (also at `/System/agent.json`).\n"
        "- Run `help` (grouped, scrollable) or `commands` (full list), `man` for the manual.\n"
        "- Read the filesystem with `ls`, `cd`, `cat`/`more`. Check the network with `net`.\n"
        "- **Everything is a file**: apps live in `/Applications` - `open <app>` launches them.\n"
        "- Edit code in **Build** (`build <file>`): line numbers, syntax highlighting, find.\n"
        "- Output is mirrored to the serial console (COM1) for headless control.\n\n"
        "## What it can do\n"
        "Files (create/edit/move/delete, persistent), networking (DNS/ping/TCP/HTTP, a text\n"
        "browser, `download <url> <dest>`), graphics (a linear framebuffer), and apps (Build\n"
        "editor, explorer, paint that saves to Pictures, clock, calculator, games). System:\n"
        "boot/setup/update/shutdown/restart/panic screens, passcode login, factory `reset`.\n\n"
        "## What it CANNOT do (be honest)\n"
        "No Linux syscall layer, no userland process model, no libc to link against - so external\n"
        "binaries (Python, Homebrew, apt/brew packages) cannot run. Every command is compiled\n"
        "into the kernel. A Linux-compatibility layer is planned but not built yet.\n";
    fs_write("/System/AGENTS.md", MD, sizeof(MD) - 1);
    static const char LLMS[] =
        "# llms.txt - HisokaOS\n"
        "HisokaOS is a from-scratch 32-bit x86 operating system.\n"
        "For a full machine-readable descriptor, run the `agent` command\n"
        "or read /System/agent.json. Human/agent brief: /System/AGENTS.md\n";
    fs_write("/etc/llms.txt", LLMS, sizeof(LLMS) - 1);
}
