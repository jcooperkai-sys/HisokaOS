/* printf.h - tiny formatted printer: %c %s %d %u %x %p %%. Writes to VGA + serial. */
#ifndef HISOKA_PRINTF_H
#define HISOKA_PRINTF_H
void kprintf(const char *fmt, ...);
void kputs(const char *s);
void kquiet(int on);            /* 1 = suppress VGA output (serial still flows), 0 = restore */
#endif
