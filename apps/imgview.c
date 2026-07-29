/* imgview.c - a real image viewer. Reads a BMP from the filesystem, switches to the
 * pixel framebuffer, and paints it centered. BMP is what 'download' and the Chromium
 * helper produce, so any image saved into the OS can be viewed with actual pixels. */
#include "imgview.h"
#include "gfx.h"
#include "ramfs.h"
#include "keyboard.h"
#include "printf.h"
#include "vga.h"
#include "string.h"
#include "types.h"

static uint32_t rd_u32(const uint8_t *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }

static const char *base_of(const char *p) { const char *b = p; for (const char *q = p; *q; q++) if (*q == '/') b = q + 1; return b; }
static int ends_with(const char *s, const char *suf) {
    int ls = (int)strlen(s), lf = (int)strlen(suf);
    if (lf > ls) return 0;
    for (int i = 0; i < lf; i++) { char a = s[ls-lf+i], b = suf[i]; if (a>='A'&&a<='Z') a+=32; if (a != b) return 0; }
    return 1;
}
int imgview_is_image(const char *name) {
    return ends_with(name, ".bmp") || ends_with(name, ".img") || ends_with(name, ".dib");
}

void imgview_run(const char *path) {
    fs_file_t *f = fs_find(path);
    if (!f || f->is_dir) { kprintf("img: %s: not found\n", base_of(path)); return; }
    uint8_t *bmp = f->data;
    int bmplen = (int)f->len;
    if (bmplen < 54 || bmp[0] != 'B' || bmp[1] != 'M') {
        kprintf("img: %s is not a BMP image. Only BMP (24/32-bit) is supported -\n", base_of(path));
        kputs("     PNG and JPEG are compressed formats this OS can't decode yet.\n");
        return;
    }
    if (!gfx_available()) { kputs("img: graphics mode unavailable (no adapter)\n"); return; }

    uint32_t off = rd_u32(bmp + 10);
    int32_t  w   = (int32_t)rd_u32(bmp + 18);
    int32_t  h   = (int32_t)rd_u32(bmp + 22);
    int topdown = h < 0; if (h < 0) h = -h;
    int bpp = bmp[28] | (bmp[29] << 8);
    if (bpp != 24 && bpp != 32) { kprintf("img: unsupported %d-bit BMP\n", bpp); return; }
    int bytespp = bpp / 8;
    int rowsize = ((bpp * w + 31) / 32) * 4;

    if (!gfx_enter(1024, 768)) { kputs("img: could not switch to graphics mode\n"); return; }
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
