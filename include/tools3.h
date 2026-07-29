/* tools3.h - a third batch of real, boxed utility apps (no games). */
#ifndef HISOKA_TOOLS3_H
#define HISOKA_TOOLS3_H
void worldclock_run(void);  /* current time across time zones                */
void nettools_run(void);    /* ping / dns / network status in one place       */
void diskusage_run(void);   /* filesystem usage by top-level folder           */
void kanban_run(void);      /* a 3-column task board: Todo / Doing / Done      */
void notes_run(void);       /* free-text sticky notes: add/edit/delete/persist */
#endif
