/* pci.h - PCI bus enumeration. The first real step toward networking: walk the
 * PCI configuration space to discover what hardware is attached (most importantly,
 * the network card) before we can write a driver for it. */
#ifndef HISOKA_PCI_H
#define HISOKA_PCI_H
#include "types.h"

typedef struct {
    uint8_t     bus, slot, func;
    uint16_t    vendor, device;     /* identifies the exact chip            */
    uint8_t     class_id, subclass; /* what kind of device it is            */
    const char *vendor_name;        /* looked up from a small known table   */
    const char *class_name;
} pci_device_t;

void          pci_scan(void);                  /* enumerate the bus into a table   */
uint32_t      pci_count(void);                 /* how many devices were found      */
pci_device_t *pci_get(uint32_t i);             /* i-th discovered device           */
pci_device_t *pci_find(uint16_t vendor, uint16_t device);  /* lookup by id, or 0   */
uint32_t      pci_config_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off);
void          pci_config_write32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off, uint32_t val);

#endif
