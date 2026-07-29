/* linker.ld — lay the kernel out in memory starting at 1 MiB (the conventional
 * load address). The .boot section (Multiboot header) must come first. */
ENTRY(_start)

SECTIONS {
    . = 1M;

    .boot : {
        *(.multiboot)
    }
    .text : {
        *(.text)
    }
    .rodata : {
        *(.rodata)
    }
    .data : {
        *(.data)
    }
    .bss : {
        *(COMMON)
        *(.bss)
    }

    kernel_end = .;     /* heap/PMM start scanning after the kernel image */
}
