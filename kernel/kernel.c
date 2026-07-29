/* kernel.c - C entry point. boot.s calls kernel_main() after setting up the stack.
 * This is the heart of HisokaOS: bring up each subsystem in order, then hand off
 * to the interactive shell. */
#include "types.h"
#include "multiboot.h"
#include "vga.h"
#include "serial.h"
#include "printf.h"
#include "gdt.h"
#include "idt.h"
#include "isr.h"
#include "pic.h"
#include "pit.h"
#include "keyboard.h"
#include "mouse.h"
#include "pmm.h"
#include "heap.h"
#include "paging.h"
#include "pci.h"
#include "rtl8139.h"
#include "ramfs.h"
#include "ata.h"
#include "persist.h"
#include "klog.h"
#include "task.h"
#include "screens.h"
#include "shell.h"

void kernel_main(uint32_t magic, multiboot_info_t *mb) {
    serial_init();
    vga_init();
    vga_titlebar("HisokaOS 0.2");

    /* The boot screen is the very first thing on the display. We draw it now (static,
     * before the timer is up) and mute the VGA side of the log so the subsystem
     * messages stream out the serial port instead of scrolling over the splash.
     * They're still captured - read them back later with 'dmesg'. */
    splash_draw();
    kquiet(1);

    kprintf("[boot] multiboot magic %x %s\n", magic,
            magic == MULTIBOOT_MAGIC ? "(ok)" : "(BAD)");

    gdt_init();      kprintf("[ ok ] GDT  - segmentation + ring0/ring3 descriptors\n");
    idt_init();      kprintf("[ ok ] IDT  - interrupt descriptor table\n");
    isr_init();      kprintf("[ ok ] ISR  - CPU exception handlers (0-31)\n");
    pic_init();      kprintf("[ ok ] PIC  - 8259 remapped to 0x20-0x2F\n");
    pit_init(100);   kprintf("[ ok ] PIT  - timer @ 100 Hz\n");
    keyboard_init(); kprintf("[ ok ] KBD  - PS/2 keyboard\n");
    mouse_init();    kprintf("[ ok ] MOUSE- PS/2 mouse (IRQ12)\n");

    uint32_t kb = (mb && magic == MULTIBOOT_MAGIC) ? mb->mem_upper : 0;
    pmm_init(kb);    kprintf("[ ok ] PMM  - %u KB RAM, %u free frames\n", kb, pmm_free_frames());
    heap_init();     kprintf("[ ok ] HEAP - kmalloc/kfree online\n");
    paging_init();   kprintf("[ ok ] VMM  - paging on (4 MiB pages, identity 0-%u MiB)\n", paging_mapped_mb());
    fs_init();       kprintf("[ ok ] RAMFS - in-memory filesystem mounted\n");
    ata_init();
    if (ata_present()) { int r = persist_load(); kprintf("[ ok ] DISK - ATA drive, %d file(s) restored ('sync' saves)\n", r < 0 ? 0 : r); }
    else                 kprintf("[ -- ] DISK - no ATA drive (files are RAM-only)\n");
    pci_scan();      kprintf("[ ok ] PCI  - %u device(s) on the bus (try 'lspci')\n", pci_count());
    rtl8139_init();
    if (rtl8139_present()) kprintf("[ ok ] NET  - RTL8139 NIC initialized (try 'net')\n");
    else                   kprintf("[ -- ] NET  - no supported NIC found\n");

    __asm__ volatile("sti");          /* interrupts on - keyboard/timer live now */
    kprintf("[ ok ] interrupts enabled\n\n");

    klog("kernel: paging, heap, filesystem, disk, PCI and network initialized");
    if (ata_present())     klog("kernel: ATA disk attached, files restored from disk");
    if (rtl8139_present()) klog("kernel: RTL8139 network card up (10.0.2.15)");
    task_init();                      /* multitasking: register the shell as task 0 */
    klog("kernel: boot complete, starting shell");

    /* Restore VGA output, then finish the boot screen. If an update was staged for
     * next boot ('update' command drops this marker), apply it now and show the
     * update screen instead of the normal spinner. */
    kquiet(0);
    if (fs_find("/System/update.pending")) {
        klog("update: applying staged update on boot");
        splash_update();
        fs_delete("/System/update.pending");
        if (ata_present()) persist_save();
    } else {
        splash_spin(70);              /* finish the boot spinner */
    }

    vga_statusbar("help  lspci  net  vm  mem  ls  date          HisokaOS 0.2  i386");
    shell_run();                      /* never returns */

    for (;;) __asm__ volatile("hlt");
}
