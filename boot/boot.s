/* boot.s — the very first code that runs. GRUB/QEMU find the Multiboot header,
 * load us at 1 MiB, jump to _start. We set up a stack and call kernel_main(). */
.set ALIGN,    1<<0              /* align loaded modules on page boundaries */
.set MEMINFO,  1<<1             /* provide a memory map */
.set FLAGS,    ALIGN | MEMINFO
.set MAGIC,    0x1BADB002        /* the magic number GRUB scans for */
.set CHECKSUM, -(MAGIC + FLAGS)

.section .multiboot, "a"     /* "a" = ALLOC: force it into the loaded image, first */
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM

.section .bss
.align 16
stack_bottom:
.skip 16384                      /* 16 KiB kernel stack */
stack_top:

.section .text
.global _start
.type _start, @function
_start:
    mov $stack_top, %esp         /* set up the stack */
    push %ebx                    /* arg2: pointer to multiboot info */
    push %eax                    /* arg1: multiboot magic */
    call kernel_main             /* into C */
    cli                          /* if it ever returns, halt forever */
hang:
    hlt
    jmp hang
.size _start, . - _start
