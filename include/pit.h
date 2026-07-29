/* pit.h - Programmable Interval Timer (system tick). */
#ifndef HISOKA_PIT_H
#define HISOKA_PIT_H
#include "types.h"
void     pit_init(uint32_t hz);
uint32_t pit_ticks(void);
#endif
