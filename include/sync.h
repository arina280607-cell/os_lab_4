#ifndef OS_LAB4_SYNC_H
#define OS_LAB4_SYNC_H

#include <signal.h>
#include <sys/types.h>

#define WORK_TIMEOUT_SECONDS 30

int sync_prepare(void);
int sync_wait(int signo, pid_t sender, int timeout_seconds);

#endif