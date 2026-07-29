/* serial.h - COM1 serial port, used for debug logging out of the VM. */
#ifndef HISOKA_SERIAL_H
#define HISOKA_SERIAL_H
void serial_init(void);
void serial_putc(char c);
void serial_write(const char *s);
#endif
