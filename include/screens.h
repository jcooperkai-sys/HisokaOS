/* screens.h - system screens: boot splash, shutdown/restart, and kernel panic. */
#ifndef HISOKA_SCREENS_H
#define HISOKA_SCREENS_H
#include "types.h"

void splash_boot(void);                          /* logo + spinner shown at boot      */
void splash_draw(void);                          /* static boot logo (no timer needed) */
void splash_spin(uint32_t ticks);                /* animate boot spinner (timer required) */
void splash_update(void);                         /* "installing update" boot screen   */
void screen_message(const char *msg, int action);/* logo + msg; 0=return 1=reboot 2=off */
void panic_sim(void);                            /* preview the panic screen, then return */
void kernel_panic(const char *msg);              /* real fatal-error screen -> auto restart */

#endif
