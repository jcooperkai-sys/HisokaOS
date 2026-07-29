/* tools.h - a set of small but real utility apps for HisokaOS (no games). */
#ifndef HISOKA_TOOLS_H
#define HISOKA_TOOLS_H
void todo_run(void);      /* a persistent task list                       */
void monitor_run(void);   /* live system monitor (RAM, uptime, files...)  */
void units_run(void);     /* unit converter: temperature, length, weight  */
void stopwatch_run(void); /* stopwatch with laps                          */
void baseconv_run(void);  /* number base converter dec/hex/bin/oct        */
void passgen_run(void);   /* random password / token generator            */
void calendar_run(void);  /* month calendar, navigate months              */
void contacts_run(void);  /* address book: add/edit/delete/search/persist */
void bookmarks_run(void); /* saved sites: add/edit/delete/open/persist    */
void timer_run(void);     /* countdown timer                              */
void counter_run(void);   /* named tally counters, persist                */
void expenses_run(void);  /* expense tracker: add/total/delete/persist    */
#endif

