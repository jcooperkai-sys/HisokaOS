/* imgview.h - display a real (pixel) image file from the filesystem in graphics
 * mode. BMP (24/32-bit) is the supported format - it's what 'download' and the
 * Chromium helper produce. */
#ifndef HISOKA_IMGVIEW_H
#define HISOKA_IMGVIEW_H
void imgview_run(const char *path);
int  imgview_is_image(const char *name);   /* 1 if the name looks like a viewable image */
#endif
