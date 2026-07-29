/* procfs.h - expose live OS state as readable files under /proc (like Linux).
 * The files are regenerated with current values whenever /proc is read. */
#ifndef HISOKA_PROCFS_H
#define HISOKA_PROCFS_H
void procfs_refresh(void);   /* (re)generate the /proc files with current data */
#endif
