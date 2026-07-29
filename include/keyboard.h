/* keyboard.h - PS/2 keyboard: IRQ1 fills a ring buffer; keyboard_getc() blocks. */
#ifndef HISOKA_KEYBOARD_H
#define HISOKA_KEYBOARD_H
void keyboard_init(void);
char keyboard_getc(void);     /* blocks until a key is available */
char keyboard_trygetc(void);  /* non-blocking: returns 0 if no key waiting */
#endif
