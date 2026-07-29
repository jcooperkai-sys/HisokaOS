/* rtc.c - read date/time from the CMOS RTC (ports 0x70/0x71), converting BCD to
 * binary and waiting out any update-in-progress. */
#include "rtc.h"
#include "ports.h"

static uint8_t cmos_read(uint8_t reg) {
    outb(0x70, reg);
    return inb(0x71);
}

static int updating(void) {
    outb(0x70, 0x0A);
    return inb(0x71) & 0x80;
}

static uint8_t bcd2bin(uint8_t v) { return (v & 0x0F) + ((v >> 4) * 10); }

void rtc_now(rtc_time_t *t) {
    while (updating()) {}                  /* don't read mid-update */
    uint8_t sec = cmos_read(0x00), min = cmos_read(0x02), hour = cmos_read(0x04);
    uint8_t day = cmos_read(0x07), mon = cmos_read(0x08), yr = cmos_read(0x09);
    uint8_t regb = cmos_read(0x0B);

    if (!(regb & 0x04)) {                  /* values are BCD - convert */
        sec = bcd2bin(sec); min = bcd2bin(min);
        hour = bcd2bin(hour & 0x7F) | (hour & 0x80);
        day = bcd2bin(day); mon = bcd2bin(mon); yr = bcd2bin(yr);
    }
    t->sec = sec; t->min = min; t->hour = hour;
    t->day = day; t->month = mon; t->year = 2000 + yr;
}
