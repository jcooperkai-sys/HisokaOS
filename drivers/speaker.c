/* speaker.c - the PC speaker. PIT channel 2 (0x42) generates a square wave at the
 * requested frequency; port 0x61 bits 0-1 gate it to the speaker. Real tones. */
#include "speaker.h"
#include "ports.h"
#include "pit.h"
#include "types.h"

void speaker_tone(uint32_t freq) {
    if (!freq) { speaker_off(); return; }
    uint32_t div = 1193180u / freq;
    outb(0x43, 0xB6);                       /* channel 2, mode 3 (square wave) */
    outb(0x42, (uint8_t)(div & 0xFF));
    outb(0x42, (uint8_t)((div >> 8) & 0xFF));
    uint8_t t = inb(0x61);
    if ((t & 3) != 3) outb(0x61, t | 3);    /* gate the speaker on */
}

void speaker_off(void) { outb(0x61, inb(0x61) & ~3); }

void speaker_beep(uint32_t freq, uint32_t ms) {
    speaker_tone(freq);
    uint32_t ticks = ms / 10; if (!ticks) ticks = 1;    /* PIT @ 100 Hz -> 10 ms/tick */
    uint32_t s = pit_ticks(); while (pit_ticks() - s < ticks) __asm__ volatile("hlt");
    speaker_off();
}
