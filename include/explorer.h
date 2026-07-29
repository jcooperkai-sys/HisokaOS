/* explorer.h - a full-screen file browser for the ramfs. */
#ifndef HISOKA_EXPLORER_H
#define HISOKA_EXPLORER_H
void explorer_run(void);
/* install a handler the explorer calls when Enter opens a file. Return 1 if the
 * handler dealt with it (e.g. launched an app or ran a script), 0 to fall back to
 * the text editor. Lets "everything is a file" launch apps/scripts from the browser. */
void explorer_set_opener(int (*opener)(const char *path));
/* install a handler for the 't' key: send the selected file to the terminal as the
 * current file (so commands like 'fcv' act on it), then close the explorer. */
void explorer_set_terminal(void (*to_terminal)(const char *path));
#endif
