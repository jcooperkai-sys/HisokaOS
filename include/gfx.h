/* gfx.h - graphics mode via the Bochs/QEMU VBE interface: a real 32-bpp linear
 * framebuffer. This is the pixel foundation for a GUI and a graphical browser. */
#ifndef HISOKA_GFX_H
#define HISOKA_GFX_H
#include "types.h"

int  gfx_available(void);          /* 1 if a QEMU/Bochs VGA adapter is present */
int  gfx_enter(int w, int h);      /* switch to a w x h x 32 framebuffer       */
void gfx_exit(void);               /* return to VGA text mode                  */
void gfx_pixel(int x, int y, uint32_t color);
void gfx_fill(int x, int y, int w, int h, uint32_t color);
void gfx_demo(void);               /* draw a graphical desktop (proof of pixels) */

#endif
