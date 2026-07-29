/* pic.c - remap the two cascaded 8259 PICs so hardware IRQs 0-15 arrive as
 * interrupt vectors 0x20-0x2F instead of colliding with CPU exceptions. */
#include "pic.h"
#include "ports.h"

#define PIC1_CMD  0x20
#define PIC1_DATA 0x21
#define PIC2_CMD  0xA0
#define PIC2_DATA 0xA1
#define PIC_EOI   0x20

void pic_init(void) {
    uint8_t a1 = inb(PIC1_DATA), a2 = inb(PIC2_DATA);   /* save masks */

    outb(PIC1_CMD, 0x11); io_wait();   /* start init (ICW1) */
    outb(PIC2_CMD, 0x11); io_wait();
    outb(PIC1_DATA, 0x20); io_wait();  /* ICW2: master offset 0x20 */
    outb(PIC2_DATA, 0x28); io_wait();  /* ICW2: slave offset  0x28 */
    outb(PIC1_DATA, 0x04); io_wait();  /* ICW3: slave on IRQ2 */
    outb(PIC2_DATA, 0x02); io_wait();
    outb(PIC1_DATA, 0x01); io_wait();  /* ICW4: 8086 mode */
    outb(PIC2_DATA, 0x01); io_wait();

    outb(PIC1_DATA, a1);               /* restore masks */
    outb(PIC2_DATA, a2);
}

void pic_eoi(uint8_t irq) {
    if (irq >= 8) outb(PIC2_CMD, PIC_EOI);
    outb(PIC1_CMD, PIC_EOI);
}
