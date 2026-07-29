/* printf.c - minimal vararg formatter. Mirrors output to both VGA and serial. */
#include "printf.h"
#include "vga.h"
#include "serial.h"
#include "string.h"
#include <stdarg.h>

/* During boot we mute the VGA side so the screen can hold a clean splash while the
 * subsystem log still streams out the serial port (visible via 'dmesg'/the host). */
static int vga_quiet = 0;
void kquiet(int on) { vga_quiet = on; }

static void emit(char c) { if (!vga_quiet) vga_putc(c); serial_putc(c); }

void kputs(const char *s) { while (*s) emit(*s++); }

static void emit_uint(uint32_t v, uint32_t base, int upper) {
    char buf[33];
    const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    int i = 0;
    if (v == 0) buf[i++] = '0';
    while (v) { buf[i++] = digits[v % base]; v /= base; }
    while (i--) emit(buf[i]);
}

static void emit_int(int32_t v) {
    if (v < 0) { emit('-'); emit_uint((uint32_t)(-v), 10, 0); }
    else emit_uint((uint32_t)v, 10, 0);
}

void kprintf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    for (; *fmt; fmt++) {
        if (*fmt != '%') { emit(*fmt); continue; }
        fmt++;
        switch (*fmt) {
            case 'c': emit((char)va_arg(ap, int)); break;
            case 's': kputs(va_arg(ap, const char *)); break;
            case 'd': emit_int(va_arg(ap, int32_t)); break;
            case 'u': emit_uint(va_arg(ap, uint32_t), 10, 0); break;
            case 'x': emit_uint(va_arg(ap, uint32_t), 16, 0); break;
            case 'X': emit_uint(va_arg(ap, uint32_t), 16, 1); break;
            case 'p': emit('0'); emit('x'); emit_uint((uint32_t)(uintptr_t)va_arg(ap, void *), 16, 0); break;
            case '%': emit('%'); break;
            default:  emit('%'); emit(*fmt); break;
        }
    }
    va_end(ap);
}
