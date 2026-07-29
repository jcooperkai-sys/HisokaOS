/* isr.c - the C side of interrupt handling. CPU exceptions (0-31) print a fault
 * and halt; hardware IRQs (32-47) acknowledge the PIC and dispatch to any driver
 * handler that registered for them. */
#include "isr.h"
#include "idt.h"
#include "pic.h"
#include "printf.h"
#include "vga.h"
#include "screens.h"

extern void isr0(void);  extern void isr1(void);  extern void isr2(void);  extern void isr3(void);
extern void isr4(void);  extern void isr5(void);  extern void isr6(void);  extern void isr7(void);
extern void isr8(void);  extern void isr9(void);  extern void isr10(void); extern void isr11(void);
extern void isr12(void); extern void isr13(void); extern void isr14(void); extern void isr15(void);
extern void isr16(void); extern void isr17(void); extern void isr18(void); extern void isr19(void);
extern void isr20(void); extern void isr21(void); extern void isr22(void); extern void isr23(void);
extern void isr24(void); extern void isr25(void); extern void isr26(void); extern void isr27(void);
extern void isr28(void); extern void isr29(void); extern void isr30(void); extern void isr31(void);
extern void irq0(void);  extern void irq1(void);  extern void irq2(void);  extern void irq3(void);
extern void irq4(void);  extern void irq5(void);  extern void irq6(void);  extern void irq7(void);
extern void irq8(void);  extern void irq9(void);  extern void irq10(void); extern void irq11(void);
extern void irq12(void); extern void irq13(void); extern void irq14(void); extern void irq15(void);

static isr_t handlers[256];

static const char *exception_msg[] = {
    "Divide-by-zero", "Debug", "Non-maskable interrupt", "Breakpoint",
    "Overflow", "Bound range exceeded", "Invalid opcode", "Device not available",
    "Double fault", "Coprocessor segment overrun", "Invalid TSS", "Segment not present",
    "Stack-segment fault", "General protection fault", "Page fault", "Reserved",
    "x87 FP exception", "Alignment check", "Machine check", "SIMD FP exception",
    "Virtualization", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved",
    "Hypervisor injection", "VMM communication", "Security exception", "Reserved",
};

void register_interrupt_handler(uint8_t n, isr_t h) { handlers[n] = h; }

/* called from the assembly stub for CPU exceptions */
void isr_handler(registers_t *r) {
    if (handlers[r->int_no]) { handlers[r->int_no](r); return; }
    kernel_panic(r->int_no < 32 ? exception_msg[r->int_no] : "Unknown CPU exception");
}

/* called from the assembly stub for hardware IRQs */
void irq_handler(registers_t *r) {
    if (handlers[r->int_no]) handlers[r->int_no](r);
    pic_eoi(r->int_no - 32);            /* tell the PIC we're done */
}

void isr_init(void) {
    void (*stubs[])(void) = {
        isr0, isr1, isr2, isr3, isr4, isr5, isr6, isr7, isr8, isr9, isr10, isr11,
        isr12, isr13, isr14, isr15, isr16, isr17, isr18, isr19, isr20, isr21, isr22,
        isr23, isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31,
    };
    for (int i = 0; i < 32; i++) idt_set_gate(i, (uint32_t)stubs[i], 0x08, 0x8E);

    void (*irqs[])(void) = {
        irq0, irq1, irq2, irq3, irq4, irq5, irq6, irq7,
        irq8, irq9, irq10, irq11, irq12, irq13, irq14, irq15,
    };
    for (int i = 0; i < 16; i++) idt_set_gate(32 + i, (uint32_t)irqs[i], 0x08, 0x8E);
}
