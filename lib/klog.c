/* klog.c - system log. Buffers events in memory (so early-boot events aren't lost
 * before the filesystem exists) and, once the FS is ready, also appends each new
 * event to /var/log/system.log so it persists with `sync`. */
#include "klog.h"
#include "string.h"
#include "ramfs.h"

#define LOGCAP 16384
#define LOGPATH "/var/log/system.log"

static char buf[LOGCAP];
static int  pos;
static int  fs_ready;

static void append(const char *s) { while (*s && pos < LOGCAP - 1) buf[pos++] = *s++; buf[pos] = 0; }

void klog(const char *msg) {
    append(msg); append("\n");
    if (fs_ready) { fs_append(LOGPATH, msg, strlen(msg)); fs_append(LOGPATH, "\n", 1); }
}

void klog_fs_ready(void) {
    fs_ready = 1;
    fs_write(LOGPATH, buf, (size_t)pos);          /* seed the file with the boot log */
}

const char *klog_buffer(void) { return buf; }
