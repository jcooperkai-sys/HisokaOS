/* rtl8139.h - driver for the Realtek RTL8139 Ethernet controller (the NIC QEMU
 * emulates). Milestone 2 of networking: power the card on, reset it, and read its
 * hardware MAC address. Frame transmit/receive come next. */
#ifndef HISOKA_RTL8139_H
#define HISOKA_RTL8139_H
#include "types.h"

void           rtl8139_init(void);   /* find + bring up the card, read its MAC */
int            rtl8139_present(void);/* 1 if a card was found and initialized  */
const uint8_t *rtl8139_mac(void);    /* 6-byte hardware MAC address             */
int            rtl8139_send(const void *data, uint16_t len);   /* transmit a frame */
uint16_t       rtl8139_recv(void *out, uint16_t max);          /* poll one frame   */
void           rtl8139_flush(void);                            /* drop stale RX frames */

#endif
