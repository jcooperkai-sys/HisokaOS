/* mouse.h - PS/2 mouse on IRQ12. Tracks a cursor in text cells (0..79, 0..24) and
 * the button state. The foundation for clickable UI and a right-click menu. */
#ifndef HISOKA_MOUSE_H
#define HISOKA_MOUSE_H
void mouse_init(void);
int  mouse_present(void);
void mouse_get(int *x, int *y, int *buttons);   /* buttons: bit0 left, bit1 right, bit2 middle */
int  mouse_moved(void);                          /* 1 once since the last call, then 0 */
#endif
