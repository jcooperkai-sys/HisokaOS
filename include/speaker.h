/* speaker.h - the PC speaker: real audio tones via PIT channel 2 + port 0x61. */
#ifndef HISOKA_SPEAKER_H
#define HISOKA_SPEAKER_H
#include "types.h"
void speaker_tone(uint32_t freq);            /* start a continuous tone (0 = off) */
void speaker_off(void);
void speaker_beep(uint32_t freq, uint32_t ms); /* tone for ms milliseconds, then off */
#endif
