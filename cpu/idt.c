/* idt.c - allocate and load a 256-entry IDT. Gates are filled in by isr_init(). */
#include "idt.h"
#include "string.h"

struct idt_entry {
    uint16_t base_low;
    uint16_t sel;
    uint8_t  zero;
    uint8_t  flags;
    uint16_t base_high;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

static struct idt_entry idt[256];
static struct idt_ptr   ip;
extern void idt_load(uint32_t);   /* in idt_load.s */

void idt_set_gate(uint8_t n, uint32_t base, uint16_t sel, uint8_t flags) {
    idt[n].base_low  = base & 0xFFFF;
    idt[n].base_high = (base >> 16) & 0xFFFF;
    idt[n].sel       = sel;
    idt[n].zero      = 0;
    idt[n].flags     = flags;
}

void idt_init(void) {
    ip.limit = sizeof(idt) - 1;
    ip.base  = (uint32_t)&idt;
    memset(idt, 0, sizeof(idt));
    idt_load((uint32_t)&ip);
}
