/* task.c - the scheduler. Tasks are kernel threads kept in a fixed array; each has
 * its own stack. task_yield() round-robins to the next live task via context_switch.
 * A new task's stack is hand-crafted so the first switch "returns" into its entry
 * function, and so that if the entry returns, control falls through to task_exit. */
#include "task.h"

extern void context_switch(uint32_t *old_esp, uint32_t new_esp);

#define MAXTASK 8
#define TSTACK  8192

typedef struct { uint32_t esp; int alive; } task_t;

static task_t  tasks[MAXTASK];
static uint8_t stacks[MAXTASK][TSTACK] __attribute__((aligned(16)));
static int     ntask, curr;

void task_init(void) {
    for (int i = 0; i < MAXTASK; i++) tasks[i].alive = 0;
    tasks[0].alive = 1;          /* the boot/shell thread is task 0 */
    ntask = 1; curr = 0;
}

int task_create(void (*entry)(void)) {
    if (ntask >= MAXTASK) return -1;
    int id = ntask++;
    uint32_t *sp = (uint32_t *)(stacks[id] + TSTACK);
    *(--sp) = (uint32_t)task_exit;   /* entry's return address -> clean exit */
    *(--sp) = (uint32_t)entry;       /* context_switch ret target -> entry   */
    *(--sp) = 0;                     /* ebp */
    *(--sp) = 0;                     /* ebx */
    *(--sp) = 0;                     /* esi */
    *(--sp) = 0;                     /* edi */
    tasks[id].esp = (uint32_t)sp;
    tasks[id].alive = 1;
    return id;
}

static int next_alive(int from) {
    for (int i = 1; i <= ntask; i++) { int j = (from + i) % ntask; if (tasks[j].alive) return j; }
    return from;
}

void task_yield(void) {
    int prev = curr, n = next_alive(curr);
    if (n == prev) return;
    curr = n;
    context_switch(&tasks[prev].esp, tasks[n].esp);
}

void task_exit(void) {
    tasks[curr].alive = 0;
    int n = next_alive(curr);
    curr = n;
    uint32_t dummy;
    context_switch(&dummy, tasks[n].esp);   /* switch away for good */
}

int task_count(void) { int c = 0; for (int i = 0; i < ntask; i++) if (tasks[i].alive) c++; return c; }
int task_current(void) { return curr; }
