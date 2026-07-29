/* mouse.c - PS/2 mouse driver. The 8042 controller's auxiliary device sends 3-byte
 * packets on IRQ12: [flags][dx][dy]. We decode them into a text-cell cursor position
 * and a button bitmask. */
#include "mouse.h"
#include "ports.h"
#include "isr.h"
#include "types.h"

#define IRQ12 44

static volatile int present;
static volatile int mx = 40, my = 12;     /* cursor in text cells */
static volatile int buttons;
static volatile int moved_flag;
static int cycle, accx, accy;
static uint8_t flags, rawx;

static void mwait_in(void)  { int t = 200000; while (t-- && (inb(0x64) & 2)); }   /* input buffer clear  */
static void mwait_out(void) { int t = 200000; while (t-- && !(inb(0x64) & 1)); }  /* output buffer full  */
static void mwrite(uint8_t b) {
    mwait_in(); outb(0x64, 0xD4);   /* next byte goes to the mouse */
    mwait_in(); outb(0x60, b);
    mwait_out(); inb(0x60);         /* consume the ACK (0xFA) */
}

static void mouse_handler(registers_t *r) {
    (void)r;
    uint8_t b = inb(0x60);
    if (cycle == 0) {
        if (!(b & 0x08)) return;    /* bit3 must be 1; otherwise out of sync */
        flags = b; cycle = 1;
    } else if (cycle == 1) {
        rawx = b; cycle = 2;
    } else {
        int dx = (flags & 0x10) ? (int)rawx - 256 : rawx;
        int dy = (flags & 0x20) ? (int)b - 256 : b;
        buttons = flags & 7;
        accx += dx; accy += dy;
        while (accx >= 4) { accx -= 4; if (mx < 79) mx++; }
        while (accx <= -4) { accx += 4; if (mx > 0) mx--; }
        while (accy >= 4) { accy -= 4; if (my > 0) my--; }    /* mouse dy: up is positive */
        while (accy <= -4) { accy += 4; if (my < 24) my++; }
        moved_flag = 1;
        cycle = 0;
    }
}

void mouse_init(void) {
    mwait_in(); outb(0x64, 0xA8);          /* enable the auxiliary (mouse) device */
    mwait_in(); outb(0x64, 0x20);          /* read controller config byte */
    mwait_out(); uint8_t cfg = inb(0x60);
    cfg |= 0x02;                           /* enable IRQ12 */
    cfg &= ~0x20;                          /* enable the mouse clock */
    mwait_in(); outb(0x64, 0x60);          /* write controller config byte */
    mwait_in(); outb(0x60, cfg);
    mwrite(0xF6);                          /* set defaults */
    mwrite(0xF4);                          /* enable data reporting */
    outb(0xA1, inb(0xA1) & ~(1 << 4));     /* unmask IRQ12 on the slave PIC */
    outb(0x21, inb(0x21) & ~(1 << 2));     /* unmask the cascade (IRQ2) on the master */
    register_interrupt_handler(IRQ12, mouse_handler);
    present = 1;
}

int  mouse_present(void) { return present; }
void mouse_get(int *x, int *y, int *b) { if (x) *x = mx; if (y) *y = my; if (b) *b = buttons; }
int  mouse_moved(void) { int m = moved_flag; moved_flag = 0; return m; }
