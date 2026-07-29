/* rtl8139.c - driver for the Realtek RTL8139 Ethernet controller.
 *
 * Bring-up: find the card on PCI, read its I/O base, enable bus mastering, power
 * it on and reset it, read the hardware MAC. Then set up a receive ring buffer and
 * transmit descriptors so we can actually move Ethernet frames.
 *
 * DMA note: the NIC reads/writes physical memory directly. Our buffers live in the
 * kernel's .bss, which paging identity-maps (virtual == physical) in the low 128 MiB,
 * so we can hand the NIC a buffer's ordinary address as its DMA address. */
#include "rtl8139.h"
#include "pci.h"
#include "ports.h"

#define RTL_VENDOR  0x10EC
#define RTL_DEVICE  0x8139

/* register offsets from the I/O base */
#define REG_IDR0    0x00   /* MAC bytes 0..5                 */
#define REG_TSD0    0x10   /* transmit status (4 descriptors)*/
#define REG_TSAD0   0x20   /* transmit start address         */
#define REG_RBSTART 0x30   /* receive buffer start           */
#define REG_CAPR    0x38   /* current read pointer           */
#define REG_CR      0x37   /* command register               */
#define REG_IMR     0x3C   /* interrupt mask                 */
#define REG_RCR     0x44   /* receive config                 */
#define REG_CONFIG1 0x52   /* power management               */

#define CR_RESET    0x10
#define CR_RE       0x08   /* receiver enable    */
#define CR_TE       0x04   /* transmitter enable */
#define CR_BUFE     0x01   /* receive buffer empty */

#define RX_BUF_LEN  8192   /* 8K ring (RCR RBLEN = 00) */

static int      present = 0;
static uint16_t io_base = 0;
static uint8_t  mac[6];

/* 8K ring + 16-byte header slack + a frame of WRAP overrun room */
static uint8_t  rx_buffer[RX_BUF_LEN + 16 + 1536] __attribute__((aligned(16)));
static uint8_t  tx_buffer[4][2048]                __attribute__((aligned(4)));
static int      tx_cur;
static uint16_t rx_off;

void rtl8139_init(void) {
    present = 0;
    pci_device_t *d = pci_find(RTL_VENDOR, RTL_DEVICE);
    if (!d) return;

    uint32_t bar0 = pci_config_read32(d->bus, d->slot, d->func, 0x10);
    io_base = (uint16_t)(bar0 & 0xFFFC);
    if (!io_base) return;

    uint32_t cmd = pci_config_read32(d->bus, d->slot, d->func, 0x04);
    pci_config_write32(d->bus, d->slot, d->func, 0x04, cmd | (1u << 2));   /* bus master */

    outb(io_base + REG_CONFIG1, 0x00);                  /* power on */
    outb(io_base + REG_CR, CR_RESET);                   /* soft reset */
    for (int spin = 1000000; spin > 0; spin--)
        if (!(inb(io_base + REG_CR) & CR_RESET)) break;

    for (int i = 0; i < 6; i++) mac[i] = inb(io_base + REG_IDR0 + i);

    /* receive ring + transmit setup */
    outl(io_base + REG_RBSTART, (uint32_t)(uintptr_t)rx_buffer);
    outl(io_base + REG_IMR, 0x0000);                    /* poll, no interrupts */
    outl(io_base + REG_RCR, 0x0000008F);                /* AAP|APM|AM|AB | WRAP, 8K */
    outb(io_base + REG_CR, CR_RE | CR_TE);              /* enable RX + TX */
    rx_off = 0;
    tx_cur = 0;

    present = 1;
}

int rtl8139_send(const void *data, uint16_t len) {
    if (!present) return -1;
    if (len > 1792) len = 1792;
    int d = tx_cur; tx_cur = (tx_cur + 1) & 3;
    uint8_t *b = tx_buffer[d];
    const uint8_t *src = (const uint8_t *)data;
    for (uint16_t i = 0; i < len; i++) b[i] = src[i];
    if (len < 60) { for (uint16_t i = len; i < 60; i++) b[i] = 0; len = 60; }  /* pad to min frame */

    outl(io_base + REG_TSAD0 + d * 4, (uint32_t)(uintptr_t)b);
    outl(io_base + REG_TSD0  + d * 4, len & 0x1FFF);    /* writing length starts TX */

    for (int spin = 0; spin < 2000000; spin++)
        if (inl(io_base + REG_TSD0 + d * 4) & 0x8000) return 0;   /* TOK */
    return -2;   /* timed out */
}

/* poll for one received frame; returns its length (excluding CRC), or 0 if none */
uint16_t rtl8139_recv(void *out, uint16_t max) {
    if (!present) return 0;
    if (inb(io_base + REG_CR) & CR_BUFE) return 0;     /* ring empty */

    uint8_t *p = rx_buffer + rx_off;
    uint16_t length = (uint16_t)(p[2] | (p[3] << 8));  /* includes 4-byte CRC */
    uint16_t frame  = (length >= 4) ? (uint16_t)(length - 4) : 0;
    uint16_t copy   = frame < max ? frame : max;
    for (uint16_t i = 0; i < copy; i++) ((uint8_t *)out)[i] = p[4 + i];

    rx_off = (uint16_t)((rx_off + length + 4 + 3) & ~3);
    rx_off %= RX_BUF_LEN;
    outw(io_base + REG_CAPR, (uint16_t)(rx_off - 16));
    return copy;
}

/* discard any frames sitting in the RX ring so the next request starts clean */
void rtl8139_flush(void) {
    static uint8_t tmp[1600];
    while (rtl8139_recv(tmp, sizeof(tmp))) { }
}

int            rtl8139_present(void) { return present; }
const uint8_t *rtl8139_mac(void)     { return mac; }
