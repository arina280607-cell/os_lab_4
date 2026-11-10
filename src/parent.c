#include "shared.h"
#include "sync.h"
#include "input.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <locale.h>
#include <wchar.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static volatile sig_atomic_t stop_signal = 0;

static void request_stop(int signo)
{
    stop_signal = signo;
}

static pid_t start_child(const char *program, const char *shared_path)
{
    pid_t pid = fork();

    if (pid == -1) {
        perror("parent: fork");
        return -1;
    }

    if (pid == 0) {
        execl(program, program, shared_path, (char *)NULL);
        perror(program);
        _exit(127);
    }

    return pid;
}

static int wait_child(pid_t pid)
{
    int status;

    while (waitpid(pid, &status, 0) == -1) {
        if (errno == EINTR) {
            continue;
        }
        perror("parent: waitpid");
        return -1;
    }

    if (WIFEXITED(status)) {
        int code = WEXITSTATUS(status);
        if (code == EXIT_SUCCESS) {
            return 0;
        }
        fprintf(stderr, "[parent] child PID=%ld exited with code %d\n",
                (long)pid, code);
        return -1;
    }

    if (WIFSIGNALED(status)) {
        fprintf(stderr, "[parent] child PID=%ld terminated by signal %d\n",
                (long)pid, WTERMSIG(status));
        return -1;
    }

    fprintf(stderr, "[parent] unexpected status for PID=%ld\n", (long)pid);
    return -1;
}

static int run_stage(pid_t child_pid)
{
    if (stop_signal != 0) {
        errno = EINTR;
        return -1;
    }
    if (kill(child_pid, SIGUSR1) == -1) {
        perror("parent: send permission");
        return -1;
    }

    if (sync_wait(SIGUSR2, child_pid, WORK_TIMEOUT_SECONDS) == -1) {
        perror("parent: wait for acknowledgement");
        return -1;
    }

    return 0;
}

static void stop_child(pid_t child_pid)
{
    if (child_pid <= 0) {
        return;
    }

    if (kill(child_pid, SIGTERM) == -1 && errno != ESRCH) {
        perror("parent: stop child");
    }
}

/* Each chunk contains complete UTF-8 characters. Reserve enough room
   for the worst-case output of wcrtomb for each input byte. */
static int process_line(const char *line, size_t length,
                        struct shared_block *block,
                        pid_t child1_pid, pid_t child2_pid)
{
    const size_t input_limit = SHARED_CAPACITY / MB_LEN_MAX;
    size_t position = 0;

    while (position < length) {
        if (stop_signal != 0) {
            errno = EINTR;
            return -1;
        }
        size_t chunk_length = 0;
        mbstate_t state = {0};

        while (position + chunk_length < length) {
            wchar_t symbol;
            size_t consumed = mbrtowc(&symbol,
                                     line + position + chunk_length,
                                     length - position - chunk_length,
                                     &state);
            if (consumed == (size_t)-1 || consumed == (size_t)-2) {
                errno = EILSEQ;
                perror("parent: invalid UTF-8 input");
                return -1;
            }
            if (consumed == 0) {
                consumed = 1;
            }
            if (consumed > input_limit - chunk_length) {
                break;
            }
            chunk_length += consumed;
        }

        if (chunk_length == 0) {
            errno = EOVERFLOW;
            perror("parent: input chunk");
            return -1;
        }

        memcpy(block->data, line + position, chunk_length);
        block->length = chunk_length;
        block->kind = MESSAGE_DATA;

        if (run_stage(child1_pid) == -1 || run_stage(child2_pid) == -1) {
            return -1;
        }

        if (fwrite(block->data, 1, block->length, stdout) != block->length) {
            perror("parent: write result");
            return -1;
        }

        position += chunk_length;
    }

    if (fflush(stdout) == EOF) {
        perror("parent: fflush");
        return -1;
    }
    return 0;
}

int main(void)
{
    char shared_path[] = "/tmp/os_lab4_XXXXXX";
    int fd = -1;
    int file_exists = 0;
    int result = EXIT_FAILURE;
    struct shared_block *block = NULL;
    pid_t child1_pid = -1;
    pid_t child2_pid = -1;
    char *line = NULL;
    size_t line_capacity = 0;
    int original_input_flags = -1;

    struct sigaction stop_action = {0};
    stop_action.sa_handler = request_stop;
    if (sigemptyset(&stop_action.sa_mask) == -1 ||
        sigaction(SIGINT, &stop_action, NULL) == -1 ||
        sigaction(SIGTERM, &stop_action, NULL) == -1) {
        perror("parent: configure stop signals");
        goto cleanup;
    }

    original_input_flags = fcntl(STDIN_FILENO, F_GETFL);
    if (original_input_flags == -1) {
        perror("parent: get input flags");
        goto cleanup;
    }
    if (fcntl(STDIN_FILENO, F_SETFL, original_input_flags | O_NONBLOCK) == -1) {
        perror("parent: set nonblocking input");
        goto cleanup;
    }

    if (setlocale(LC_CTYPE, "C.UTF-8") == NULL) {
        fprintf(stderr, "parent: C.UTF-8 locale is unavailable\n");
        goto cleanup;
    }
    if (sync_prepare() == -1) {
        perror("parent: sync_prepare");
        goto cleanup;
    }

    /* Make a broken output pipe report an I/O error instead of killing
       the parent before it can clean up its children and temporary file. */
    struct sigaction action = {0};
    action.sa_handler = SIG_IGN;
    if (sigemptyset(&action.sa_mask) == -1 ||
        sigaction(SIGPIPE, &action, NULL) == -1) {
        perror("parent: configure SIGPIPE");
        goto cleanup;
    }

    fd = mkstemp(shared_path);
    if (fd == -1) {
        perror("parent: mkstemp");
        goto cleanup;
    }
    file_exists = 1;

    if (fcntl(fd, F_SETFD, FD_CLOEXEC) == -1) {
        perror("parent: fcntl FD_CLOEXEC");
        goto cleanup;
    }
    if (ftruncate(fd, (off_t)sizeof(struct shared_block)) == -1) {
        perror("parent: ftruncate");
        goto cleanup;
    }
    if (shared_map(fd, &block) == -1) {
        perror("parent: shared_map");
        goto cleanup;
    }

    if (fprintf(stderr, "[parent] PID=%ld, shared file: %s\n",
                (long)getpid(), shared_path) < 0) {
        perror("parent: diagnostic output");
        goto cleanup;
    }
    if (fflush(stderr) == EOF) {
        perror("parent: fflush stderr");
        goto cleanup;
    }

    child1_pid = start_child("./build/child1", shared_path);
    if (child1_pid == -1) {
        goto cleanup;
    }
    child2_pid = start_child("./build/child2", shared_path);
    if (child2_pid == -1) {
        goto cleanup;
    }

    if (isatty(STDIN_FILENO)) {
        if (fprintf(stderr,
                    "Enter lines. To finish, press Ctrl+D on an empty input line.\n") < 0) {
            perror("parent: prompt");
            goto cleanup;
        }
    }

    for (;;) {
        errno = 0;
        ssize_t length = input_readline(STDIN_FILENO, &line,
                                        &line_capacity, &stop_signal);

        if (length == -1) {
            if (stop_signal != 0) {
                goto cleanup;
            }
            if (errno != 0) {
                perror("parent: input_readline");
                goto cleanup;
            }
            break;
        }

        if (process_line(line, (size_t)length, block,
                         child1_pid, child2_pid) == -1) {
            goto cleanup;
        }
    }

    block->kind = MESSAGE_END;
    block->length = 0;
    if (run_stage(child1_pid) == -1 || run_stage(child2_pid) == -1) {
        goto cleanup;
    }
    result = EXIT_SUCCESS;

cleanup:
    if (stop_signal != 0) {
        result = 128 + stop_signal;
        fprintf(stderr, "[parent] stopping on signal %d; cleaning up\n",
                (int)stop_signal);
    }
    if (result != EXIT_SUCCESS) {
        stop_child(child1_pid);
        stop_child(child2_pid);
    }
    if (child1_pid > 0 && wait_child(child1_pid) == -1) {
        result = EXIT_FAILURE;
    }
    if (child2_pid > 0 && wait_child(child2_pid) == -1) {
        result = EXIT_FAILURE;
    }
    if (original_input_flags != -1 &&
        fcntl(STDIN_FILENO, F_SETFL, original_input_flags) == -1) {
        perror("parent: restore input flags");
        result = EXIT_FAILURE;
    }
    free(line);
    if (block != NULL && shared_unmap(block) == -1) {
        perror("parent: shared_unmap");
        result = EXIT_FAILURE;
    }
    if (fd != -1 && close(fd) == -1) {
        perror("parent: close");
        result = EXIT_FAILURE;
    }
    if (file_exists && unlink(shared_path) == -1) {
        perror("parent: unlink");
        result = EXIT_FAILURE;
    }
    return stop_signal ? 128 + stop_signal : result;
}
