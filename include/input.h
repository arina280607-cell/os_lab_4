#ifndef OS_LAB4_INPUT_H
#define OS_LAB4_INPUT_H

#include <signal.h>
#include <stddef.h>
#include <sys/types.h>

/* fd must be nonblocking. Returns byte count, or -1 (EOF/error).
   EOF is indicated by errno == 0. Cancellation uses EINTR. */
ssize_t input_readline(int fd, char **line, size_t *capacity,
                       const volatile sig_atomic_t *stop_signal);

#endif
