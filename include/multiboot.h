/* multiboot.h - the structure GRUB/QEMU hand us at boot (Multiboot 1 spec). */
#ifndef HISOKA_MULTIBOOT_H
#define HISOKA_MULTIBOOT_H
#include "types.h"

#define MULTIBOOT_MAGIC 0x2BADB002

typedef struct {
    uint32_t size;
    uint64_t addr;
    uint64_t len;
    uint32_t type;          /* 1 = available RAM */
} __attribute__((packed)) multiboot_mmap_entry_t;

typedef struct {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;     /* KB of RAM above 1MB */
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    uint32_t syms[4];
    uint32_t mmap_length;
    uint32_t mmap_addr;     /* address of the memory map array */
} __attribute__((packed)) multiboot_info_t;

#endif
