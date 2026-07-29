/* pmm.h - physical memory manager: tracks 4 KiB page frames with a bitmap. */
#ifndef HISOKA_PMM_H
#define HISOKA_PMM_H
#include "types.h"
void     pmm_init(uint32_t mem_upper_kb);
uint32_t pmm_alloc_frame(void);     /* returns physical address, or 0 if none */
void     pmm_free_frame(uint32_t addr);
uint32_t pmm_free_frames(void);
uint32_t pmm_total_frames(void);
#endif
