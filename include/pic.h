/* pic.h - 8259 Programmable Interrupt Controller. */
#ifndef HISOKA_PIC_H
#define HISOKA_PIC_H
#include "types.h"
void pic_init(void);          /* remap IRQs 0-15 to interrupt vectors 0x20-0x2F */
void pic_eoi(uint8_t irq);    /* end-of-interrupt acknowledgement */
#endif
