/* paging.h - virtual memory. Turns on the MMU and installs a page-fault handler
 * so bad memory accesses are reported and contained instead of triple-faulting. */
#ifndef HISOKA_PAGING_H
#define HISOKA_PAGING_H
#include "types.h"

void     paging_init(void);       /* build the page directory and enable paging */
int      paging_enabled(void);    /* 1 once the MMU is on */
uint32_t paging_mapped_mb(void);  /* size of the identity-mapped region, in MiB */
void     paging_identity_map(uint32_t base, uint32_t size);  /* map extra phys (framebuffer) */

#endif
