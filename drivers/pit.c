/* pit.c - set the 8253/8254 timer to fire IRQ0 at a chosen frequency, and count
 * ticks (HisokaOS uptime). */
#include "pit.h"
#include "ports.h"
#include "isr.h"

static volatile uint32_t ticks;

static void on_tick(registers_t *r) { (void)r; ticks++; }

void pit_init(uint32_t hz) {
    register_interrupt_handler(IRQ0, on_tick);
    uint32_t divisor = 1193180 / hz;
    outb(0x43, 0x36);                         /* channel 0, lobyte/hibyte, mode 3 */
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));
}

uint32_t pit_ticks(void) { return ticks; }
