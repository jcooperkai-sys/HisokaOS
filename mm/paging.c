/* paging.c - virtual memory via 4 MiB pages (PSE).
 *
 * We identity-map the low 128 MiB (virtual address == physical address) so every
 * pointer the kernel already holds - code, the PMM bitmap, the heap, the VGA text
 * buffer at 0xB8000 - keeps working the instant the MMU comes on. With that safe
 * mapping in place we set CR3 to our page directory and flip CR0.PG.
 *
 * This is the groundwork for real memory protection: anything OUTSIDE the mapped
 * range now generates a page fault (#PF, interrupt 14) instead of silently reading
 * garbage. Our handler reports the faulting address from CR2 and halts, so a stray
 * access is contained rather than corrupting the machine. Later milestones will
 * give each process its own directory for true isolation. */
#include "paging.h"
#include "isr.h"
#include "printf.h"
#include "vga.h"

#define PAGE_PRESENT 0x001u
#define PAGE_RW      0x002u
#define PAGE_PS      0x080u      /* page-size bit: this PDE maps a 4 MiB page */
#define PD_ENTRIES   1024
#define MAP_MB       128         /* identity-map [0, 128 MiB) - covers all QEMU RAM */

/* The page directory must be 4 KiB aligned. Each entry maps a 4 MiB region. */
static uint32_t page_directory[PD_ENTRIES] __attribute__((aligned(4096)));
static int enabled = 0;

/* #PF handler - registered for interrupt 14, overriding the default exception
 * printer so we can also report CR2 (the address that faulted) and decode the
 * error code. */
static void page_fault(registers_t *r) {
    uint32_t cr2;
    __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
    vga_setcolor(VGA_WHITE, VGA_RED);
    kprintf("\n*** PAGE FAULT @ %p  [%s | %s | %s]  err=%u - halting ***\n",
            (void *)cr2,
            (r->err_code & 0x1) ? "protection-violation" : "page-not-present",
            (r->err_code & 0x2) ? "on-write"             : "on-read",
            (r->err_code & 0x4) ? "user-mode"            : "kernel-mode",
            r->err_code);
    for (;;) __asm__ volatile("cli; hlt");
}

void paging_init(void) {
    for (int i = 0; i < PD_ENTRIES; i++) page_directory[i] = 0;          /* all unmapped */
    for (int i = 0; i < (MAP_MB / 4); i++)                                /* 4 MiB strides */
        page_directory[i] = (uint32_t)(i * 0x400000u) | PAGE_PRESENT | PAGE_RW | PAGE_PS;

    register_interrupt_handler(14, page_fault);

    /* CR4.PSE = 1  -> enable 4 MiB pages */
    uint32_t cr4;
    __asm__ volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= 0x00000010u;
    __asm__ volatile("mov %0, %%cr4" :: "r"(cr4));

    /* CR3 = physical address of the page directory (identity-mapped, so == &pd) */
    __asm__ volatile("mov %0, %%cr3" :: "r"((uint32_t)page_directory));

    /* CR0.PG = 1 -> turn the MMU on. From here every access is translated. */
    uint32_t cr0;
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000u;
    __asm__ volatile("mov %0, %%cr0" :: "r"(cr0));

    enabled = 1;
}

/* identity-map an extra physical region (e.g. a graphics framebuffer that lives
 * above the 128 MiB we mapped at boot) so the kernel can write to it. */
void paging_identity_map(uint32_t base, uint32_t size) {
    uint32_t start = base & ~0x3FFFFFu;
    uint32_t end   = (base + size + 0x3FFFFFu) & ~0x3FFFFFu;
    for (uint32_t a = start; a >= start && a < end; a += 0x400000u) {
        uint32_t idx = a / 0x400000u;
        if (idx < PD_ENTRIES) page_directory[idx] = a | PAGE_PRESENT | PAGE_RW | PAGE_PS;
        if (a + 0x400000u < a) break;   /* wrap guard */
    }
    __asm__ volatile("mov %0, %%cr3" :: "r"((uint32_t)page_directory));   /* flush TLB */
}

int      paging_enabled(void)    { return enabled; }
uint32_t paging_mapped_mb(void)  { return MAP_MB; }
