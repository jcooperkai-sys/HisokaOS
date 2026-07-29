/* gdt.h - Global Descriptor Table: defines flat kernel (ring0) and user (ring3)
 * code/data segments. The ring3 descriptors are the foundation of HisokaOS's
 * privilege separation (user code can never touch kernel memory directly). */
#ifndef HISOKA_GDT_H
#define HISOKA_GDT_H
void gdt_init(void);
#endif
