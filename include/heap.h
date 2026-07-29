/* heap.h - kernel dynamic memory. */
#ifndef HISOKA_HEAP_H
#define HISOKA_HEAP_H
#include "types.h"
void   heap_init(void);
void  *kmalloc(size_t n);
void   kfree(void *p);
size_t heap_used(void);
#endif
