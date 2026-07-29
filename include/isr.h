/* isr.h - interrupt service routines. The assembly stubs save CPU state into a
 * `registers_t` and call into C. Drivers register handlers for specific IRQs. */
#ifndef HISOKA_ISR_H
#define HISOKA_ISR_H
#include "types.h"

typedef struct {
    uint32_t ds;                                     /* pushed by our stub */
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax; /* pushad */
    uint32_t int_no, err_code;                       /* stub + CPU */
    uint32_t eip, cs, eflags, useresp, ss;           /* pushed by CPU */
} __attribute__((packed)) registers_t;

typedef void (*isr_t)(registers_t *);

void isr_init(void);
void register_interrupt_handler(uint8_t n, isr_t handler);

#define IRQ0  32   /* PIT timer */
#define IRQ1  33   /* keyboard  */

#endif
