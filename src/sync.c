#include "sync.h"

#include <errno.h>
#include <time.h>

int sync_prepare(void)
{
    sigset_t signals;

    if (sigemptyset(&signals) == -1 ||
        sigaddset(&signals, SIGUSR1) == -1 ||
        sigaddset(&signals, SIGUSR2) == -1) {
        return -1;
    }

    return sigprocmask(SIG_BLOCK, &signals, NULL);
}

int sync_wait(int signo, pid_t sender, int timeout_seconds)
{
    if (sender <= 0 || timeout_seconds < 0) {
        errno = EINVAL;
        return -1;
    }

    sigset_t signals;

    if (sigemptyset(&signals) == -1 ||
        sigaddset(&signals, signo) == -1) {
        return -1;
    }

    siginfo_t info;
    int received;
    struct timespec timeout = {
        .tv_sec = timeout_seconds,
        .tv_nsec = 0
    };

    if (timeout_seconds == 0) {
        received = sigwaitinfo(&signals, &info);
    } else {
        received = sigtimedwait(&signals, &info, &timeout);
    }

    if (received == -1) {
        return -1;
    }

    if (info.si_pid != sender) {
        errno = EPERM;
        return -1;
    }

    return 0;
}
