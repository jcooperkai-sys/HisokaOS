/* browser.c - a real (streamed) Chromium browser.
 *
 * HisokaOS can't run Chromium itself, but it can DISPLAY it: the host runs headless
 * Chromium (chrome-server.py) and serves a rendered BMP of any URL. We download
 * that over our TCP/IP stack (from 10.0.2.2:8090, the host), decode the BMP, switch
 * to the pixel framebuffer, and paint it. Real Chromium pages, in our from-scratch
 * OS. Press any key to return to text. */
#include "browser.h"
#include "net.h"
#include "gfx.h"
#include "keyboard.h"
#include "printf.h"
#include "types.h"

#define BUFSZ (2 * 1024 * 1024)     /* 2 MiB - holds the rendered BMP */
static char buf[BUFSZ];

static uint32_t rd_u32(const uint8_t *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }

void browser_run(const char *url) {
    if (!gfx_available()) { kputs("browse: needs graphics mode (no adapter found)\n"); return; }
    if (!url || !*url) url = "example.com";

    char path[200]; int n = 0;
    const char *pre = "/render?url=";
    while (*pre) path[n++] = *pre++;
    for (const char *p = url; *p && n < 198; p++) path[n++] = *p;
    path[n] = 0;

    kprintf("browse: asking host Chromium to render %s ...\n", url);
    int len = net_http_get_ep("10.0.2.2", 8090, path, buf, BUFSZ);
    if (len <= 0) { kprintf("browse: no response (err %d). Is chrome-server.py running on the host?\n", len); return; }

    int body = -1;
    for (int i = 0; i + 3 < len; i++)
        if (buf[i]=='\r' && buf[i+1]=='\n' && buf[i+2]=='\r' && buf[i+3]=='\n') { body = i + 4; break; }
    if (body < 0) { kputs("browse: malformed response\n"); return; }

    uint8_t *bmp = (uint8_t *)buf + body;
    int bmplen = len - body;
    if (bmplen < 54 || bmp[0] != 'B' || bmp[1] != 'M') { kprintf("browse: not an image (%d bytes)\n", bmplen); return; }

    uint32_t off = rd_u32(bmp + 10);
    int32_t  w   = (int32_t)rd_u32(bmp + 18);
    int32_t  h   = (int32_t)rd_u32(bmp + 22);
    int topdown = h < 0; if (h < 0) h = -h;
    int bpp = bmp[28] | (bmp[29] << 8);
    if (bpp != 24 && bpp != 32) { kprintf("browse: unsupported bpp %d\n", bpp); return; }
    int bytespp = bpp / 8;
    int rowsize = ((bpp * w + 31) / 32) * 4;

    if (!gfx_enter(1024, 768)) { kputs("browse: graphics failed\n"); return; }
    int ox = (1024 - w) / 2; if (ox < 0) ox = 0;
    int oy = (768 - h) / 2;  if (oy < 0) oy = 0;
    for (int y = 0; y < h; y++) {
        int sy = topdown ? y : (h - 1 - y);
        uint32_t roff = off + (uint32_t)sy * rowsize;
        if (roff + (uint32_t)w * bytespp > (uint32_t)bmplen) break;
        uint8_t *row = bmp + roff;
        for (int x = 0; x < w; x++) {
            uint8_t b = row[x*bytespp], g = row[x*bytespp+1], r = row[x*bytespp+2];
            gfx_pixel(ox + x, oy + y, ((uint32_t)r << 16) | ((uint32_t)g << 8) | b);
        }
    }
    keyboard_getc();
    gfx_exit();
}
