/* pci.c - PCI configuration-space enumeration.
 *
 * PCI config space is reached through two I/O ports: write a 32-bit address
 * (bus/slot/func/register, with bit 31 set) to 0xCF8, then read/write the data at
 * 0xCFC. We walk bus 0, every slot and function, and record each device that
 * answers (vendor id != 0xFFFF). This is how the kernel discovers its hardware -
 * and, crucially, finds the network card so a later milestone can drive it. */
#include "pci.h"
#include "ports.h"

#define CONFIG_ADDRESS 0xCF8
#define CONFIG_DATA    0xCFC
#define MAX_DEVICES    48

static pci_device_t devices[MAX_DEVICES];
static uint32_t      device_count;

uint32_t pci_config_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off) {
    uint32_t address = (1u << 31) | ((uint32_t)bus << 16) | ((uint32_t)slot << 11) |
                       ((uint32_t)func << 8) | (off & 0xFC);
    outl(CONFIG_ADDRESS, address);
    return inl(CONFIG_DATA);
}

void pci_config_write32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off, uint32_t val) {
    uint32_t address = (1u << 31) | ((uint32_t)bus << 16) | ((uint32_t)slot << 11) |
                       ((uint32_t)func << 8) | (off & 0xFC);
    outl(CONFIG_ADDRESS, address);
    outl(CONFIG_DATA, val);
}

/* a small, honest lookup table - only chips we actually expect under QEMU */
static const char *vendor_name(uint16_t v) {
    switch (v) {
        case 0x8086: return "Intel";
        case 0x10EC: return "Realtek";
        case 0x1022: return "AMD";
        case 0x1234: return "QEMU/Bochs VGA";
        case 0x1AF4: return "Red Hat/Virtio";
        case 0x106B: return "Apple";
        default:     return "Unknown";
    }
}

static const char *class_name(uint8_t class_id, uint8_t subclass) {
    switch (class_id) {
        case 0x00: return "Unclassified";
        case 0x01: return "Storage controller";
        case 0x02: return "Network controller";
        case 0x03: return "Display controller";
        case 0x04: return "Multimedia";
        case 0x06: return (subclass == 0x00) ? "Host bridge" :
                          (subclass == 0x01) ? "ISA bridge"  : "Bridge";
        case 0x0C: return "Serial bus (USB/etc.)";
        default:   return "Other";
    }
}

void pci_scan(void) {
    device_count = 0;
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            for (uint8_t func = 0; func < 8; func++) {
                uint32_t id = pci_config_read32((uint8_t)bus, slot, func, 0x00);
                uint16_t vendor = id & 0xFFFF;
                if (vendor == 0xFFFF) { if (func == 0) break; else continue; }  /* nothing here */

                uint32_t classes = pci_config_read32((uint8_t)bus, slot, func, 0x08);
                if (device_count < MAX_DEVICES) {
                    pci_device_t *d = &devices[device_count++];
                    d->bus = (uint8_t)bus; d->slot = slot; d->func = func;
                    d->vendor = vendor;
                    d->device = (id >> 16) & 0xFFFF;
                    d->class_id = (classes >> 24) & 0xFF;
                    d->subclass = (classes >> 16) & 0xFF;
                    d->vendor_name = vendor_name(d->vendor);
                    d->class_name  = class_name(d->class_id, d->subclass);
                }
            }
        }
    }
}

uint32_t      pci_count(void)        { return device_count; }
pci_device_t *pci_get(uint32_t i)    { return i < device_count ? &devices[i] : (pci_device_t *)0; }

pci_device_t *pci_find(uint16_t vendor, uint16_t device) {
    for (uint32_t i = 0; i < device_count; i++)
        if (devices[i].vendor == vendor && devices[i].device == device) return &devices[i];
    return (pci_device_t *)0;
}
