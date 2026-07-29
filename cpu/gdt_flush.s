/* gdt_flush.s — load the GDT register and reload all segment registers so the
 * new descriptors take effect. The far jump reloads CS. */
.global gdt_flush
.type gdt_flush, @function
gdt_flush:
    mov 4(%esp), %eax
    lgdt (%eax)
    mov $0x10, %ax        /* 0x10 = kernel data segment */
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss
    ljmp $0x08, $.flush    /* 0x08 = kernel code segment; far jump reloads CS */
.flush:
    ret
