/* task.h - kernel multitasking: cooperative round-robin scheduling between tasks,
 * each with its own kernel stack, switched by the assembly context_switch. */
#ifndef HISOKA_TASK_H
#define HISOKA_TASK_H
#include "types.h"

void task_init(void);                       /* register the boot thread as task 0 */
int  task_create(void (*entry)(void));      /* spawn a task; returns its id or -1  */
void task_yield(void);                       /* give the CPU to the next task       */
void task_exit(void);                        /* end the current task (never returns)*/
int  task_count(void);                       /* number of live tasks                */
int  task_current(void);                     /* id of the running task              */

#endif
