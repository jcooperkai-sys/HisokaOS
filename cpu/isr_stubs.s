/* isr_stubs.s — one tiny stub per vector. Each pushes a (dummy) error code and
 * the vector number, then jumps to a common routine that saves all registers,
 * switches to kernel data segment, and calls into C. */

.macro ISR_NOERR num
.global isr\num
.type isr\num, @function
isr\num:
    cli
    push $0
    push $\num
    jmp isr_common
.endm

.macro ISR_ERR num
.global isr\num
.type isr\num, @function
isr\num:
    cli
    push $\num
    jmp isr_common
.endm

.macro IRQ num, vec
.global irq\num
.type irq\num, @function
irq\num:
    cli
    push $0
    push $\vec
    jmp irq_common
.endm

/* CPU exceptions: vectors with a hardware error code vs. those without */
ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_ERR   21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_ERR   30
ISR_NOERR 31

/* hardware IRQs remapped to 32..47 */
IRQ 0, 32
IRQ 1, 33
IRQ 2, 34
IRQ 3, 35
IRQ 4, 36
IRQ 5, 37
IRQ 6, 38
IRQ 7, 39
IRQ 8, 40
IRQ 9, 41
IRQ 10, 42
IRQ 11, 43
IRQ 12, 44
IRQ 13, 45
IRQ 14, 46
IRQ 15, 47

.extern isr_handler
.extern irq_handler

isr_common:
    pusha
    mov %ds, %eax
    push %eax              /* save the data segment selector */
    mov $0x10, %ax         /* load kernel data segment */
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    push %esp              /* pass pointer to registers_t */
    call isr_handler
    add $4, %esp
    pop %eax               /* restore data segment */
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    popa
    add $8, %esp           /* discard pushed int_no + err_code */
    sti
    iret

irq_common:
    pusha
    mov %ds, %eax
    push %eax
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    push %esp
    call irq_handler
    add $4, %esp
    pop %eax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    popa
    add $8, %esp
    sti
    iret
