/* rtc.h - CMOS real-time clock. */
#ifndef HISOKA_RTC_H
#define HISOKA_RTC_H
#include "types.h"
typedef struct { uint8_t sec, min, hour, day, month; uint16_t year; } rtc_time_t;
void rtc_now(rtc_time_t *t);
#endif
