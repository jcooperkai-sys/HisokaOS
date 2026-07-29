/* idt_load.s — load the IDT register with our table pointer. */
.global idt_load
.type idt_load, @function
idt_load:
    mov 4(%esp), %eax
    lidt (%eax)
    ret
