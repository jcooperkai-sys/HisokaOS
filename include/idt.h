/* idt.h - Interrupt Descriptor Table: maps interrupt/exception vectors to handlers. */
#ifndef HISOKA_IDT_H
#define HISOKA_IDT_H
#include "types.h"
void idt_init(void);
void idt_set_gate(uint8_t n, uint32_t handler, uint16_t sel, uint8_t flags);
#endif
