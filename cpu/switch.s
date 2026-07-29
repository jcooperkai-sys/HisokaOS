/* switch.s - the context switch. Saves the current task's callee-saved registers
 * and stack pointer, then loads the next task's stack and registers and returns
 * into it. This is the heart of multitasking. (AT&T syntax, cdecl.)
 *
 *   void context_switch(uint32_t *old_esp, uint32_t new_esp);
 */
.global context_switch
.type context_switch, @function
context_switch:
    mov 4(%esp), %eax      /* eax = old_esp (where to save current esp) */
    mov 8(%esp), %edx      /* edx = new_esp (stack to switch to)        */
    push %ebp
    push %ebx
    push %esi
    push %edi
    mov %esp, (%eax)       /* *old_esp = current esp */
    mov %edx, %esp         /* switch stacks          */
    pop %edi
    pop %esi
    pop %ebx
    pop %ebp
    ret                    /* return into the new task */
