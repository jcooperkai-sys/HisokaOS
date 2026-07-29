/* gfx.c - real graphics mode through the Bochs/QEMU VBE "DISPI" interface.
 *
 * QEMU's standard VGA exposes a linear framebuffer we can drive directly: write
 * the resolution/bpp to the DISPI registers (I/O ports 0x1CE/0x1CF), enable the
 * linear framebuffer, and find its physical address from the VGA card's PCI BAR0.
 * After mapping that framebuffer into the page tables we can plot pixels. This is
 * the foundation for a graphical UI and a streamed (host-rendered) browser. */
#include "gfx.h"
#include "pci.h"
#include "ports.h"
#include "paging.h"

#define DISPI_INDEX   0x1CE
#define DISPI_DATA    0x1CF
#define DISPI_XRES    1
#define DISPI_YRES    2
#define DISPI_BPP     3
#define DISPI_ENABLE  4
#define DISPI_ENABLED 0x01
#define DISPI_LFB     0x40

static volatile uint32_t *lfb;
static int W, H;

static void dispi(uint16_t idx, uint16_t val) { outw(DISPI_INDEX, idx); outw(DISPI_DATA, val); }

int gfx_available(void) { return pci_find(0x1234, 0x1111) != 0; }   /* QEMU/Bochs VGA */

int gfx_enter(int w, int h) {
    pci_device_t *d = pci_find(0x1234, 0x1111);
    if (!d) return 0;
    uint32_t bar0 = pci_config_read32(d->bus, d->slot, d->func, 0x10) & 0xFFFFFFF0u;
    if (!bar0) return 0;
    paging_identity_map(bar0, (uint32_t)w * (uint32_t)h * 4u + 0x400000u);
    lfb = (volatile uint32_t *)bar0;
    dispi(DISPI_ENABLE, 0);
    dispi(DISPI_XRES, (uint16_t)w);
    dispi(DISPI_YRES, (uint16_t)h);
    dispi(DISPI_BPP, 32);
    dispi(DISPI_ENABLE, DISPI_ENABLED | DISPI_LFB);
    W = w; H = h;
    return 1;
}
void gfx_exit(void) { dispi(DISPI_ENABLE, 0); }   /* disable VBE -> legacy VGA text */

void gfx_pixel(int x, int y, uint32_t c) { if ((unsigned)x < (unsigned)W && (unsigned)y < (unsigned)H) lfb[y * W + x] = c; }
void gfx_fill(int x, int y, int w, int h, uint32_t c) {
    for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) gfx_pixel(x + i, y + j, c);
}

void gfx_demo(void) {
    if (!gfx_enter(1024, 768)) return;
    /* a desktop gradient (blue near the top, fading to black) */
    for (int y = 0; y < H; y++) {
        int b = 70 - y * 70 / H; if (b < 0) b = 0;
        uint32_t col = (uint32_t)b | ((uint32_t)(b / 2) << 8);
        for (int x = 0; x < W; x++) lfb[y * W + x] = col;
    }
    /* a window with the HisokaOS blue chrome border - now drawn in pixels */
    int wx = 212, wy = 160, ww = 600, wh = 448;
    gfx_fill(wx, wy, ww, wh, 0x000000C0);
    gfx_fill(wx + 10, wy + 30, ww - 20, wh - 40, 0x00101018);
    /* a big green H (for Hisoka), built from rectangles */
    int hx = wx + 70, hy = wy + 100;
    gfx_fill(hx,        hy, 28, 200, 0x0000DD00);
    gfx_fill(hx + 150,  hy, 28, 200, 0x0000DD00);
    gfx_fill(hx,        hy + 86, 178, 28, 0x0000DD00);
    /* true-color swatches to prove 24-bit color */
    uint32_t sw[6] = { 0x00FF4040, 0x0040FF40, 0x004080FF, 0x00FFFF40, 0x0040FFFF, 0x00FF40FF };
    for (int i = 0; i < 6; i++) gfx_fill(wx + 300 + i * 44, wy + 120, 36, 200, sw[i]);
}
