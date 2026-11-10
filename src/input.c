#include "input.h"

#include <errno.h>
#include <limits.h>
#include <poll.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* One input stream, used only by the parent. */
static char pending[4096];
static size_t pending_pos;
static size_t pending_size;

ssize_t input_readline(int fd, char **line, size_t *capacity,
                       const volatile sig_atomic_t *stop_signal)
{
    size_t length = 0;

    for (;;) {
        if (*stop_signal != 0) {
            errno = EINTR;
            return -1;
        }

        if (pending_pos < pending_size) {
            size_t amount = pending_size - pending_pos;
            char *newline = memchr(pending + pending_pos, '\n', amount);
            if (newline != NULL) {
                amount = (size_t)(newline - (pending + pending_pos)) + 1;
            }
            if (amount > (size_t)SSIZE_MAX - length) {
                errno = EOVERFLOW;
                return -1;
            }
            size_t needed = length + amount;
            if (needed > *capacity) {
                size_t next = *capacity ? *capacity : 256;
                while (next < needed) {
                    if (next > SIZE_MAX / 2) {
                        next = needed;
                        break;
                    }
                    next *= 2;
                }
                char *grown = realloc(*line, next);
                if (grown == NULL) {
                    return -1;
                }
                *line = grown;
                *capacity = next;
            }
            memcpy(*line + length, pending + pending_pos, amount);
            length += amount;
            pending_pos += amount;
            if (newline != NULL) {
                return (ssize_t)length;
            }
            continue;
        }

        struct pollfd input = {
            .fd = fd,
            .events = POLLIN
        };
        /* The bounded sleep also catches cancellation that arrives just
           before poll. Nonblocking read avoids a second blocking point. */
        int ready = poll(&input, 1, 100);
        if (ready == -1) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (ready == 0) {
            continue;
        }
        if (input.revents & POLLNVAL) {
            errno = EBADF;
            return -1;
        }
        if (*stop_signal != 0) {
            errno = EINTR;
            return -1;
        }

        ssize_t count = read(fd, pending, sizeof pending);
        if (count == -1) {
            if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
            return -1;
        }
        if (count == 0) {
            errno = 0;
            return length ? (ssize_t)length : -1;
        }
        pending_pos = 0;
        pending_size = (size_t)count;
    }
}
