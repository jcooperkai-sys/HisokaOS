/* ui.h - shared TUI toolkit for HisokaOS apps: framed boxes, panels, tiles/cards
 * and a scrollable text box. Lets apps look polished (not plain terminal text). */
#ifndef HISOKA_UI_H
#define HISOKA_UI_H
#include "types.h"

void ui_fill(int x, int y, int w, int h, uint8_t bg);                       /* fill a rect with bg spaces       */
void ui_box(int x, int y, int w, int h, uint8_t fg, uint8_t bg);           /* single-line frame                */
void ui_dbox(int x, int y, int w, int h, uint8_t fg, uint8_t bg);          /* double-line frame                */
void ui_panel(int x, int y, int w, int h, const char *title, uint8_t fg, uint8_t bg); /* framed + filled + title */
void ui_text(int x, int y, const char *s, uint8_t fg, uint8_t bg);
void ui_textn(int x, int y, const char *s, int maxw, uint8_t fg, uint8_t bg);
void ui_center(int y, int x0, int w, const char *s, uint8_t fg, uint8_t bg);
void ui_card(int x, int y, int w, const char *label, int selected, uint8_t accent); /* a clickable-looking tile */
void ui_hint(const char *s);                                                /* themed bottom hint bar            */
void ui_scrollbox_view(const char *title, const char *text);                /* scrollable text in a box (q quit) */

#endif
