/* klog.h - the system log. Events are appended to an in-memory buffer during boot
 * and mirrored to /var/log/system.log once the filesystem is up. */
#ifndef HISOKA_KLOG_H
#define HISOKA_KLOG_H
void        klog(const char *msg);        /* record one event line */
void        klog_fs_ready(void);          /* flush buffer to /var/log/system.log */
const char *klog_buffer(void);            /* the in-memory log text */
#endif
