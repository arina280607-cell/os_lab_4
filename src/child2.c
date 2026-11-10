#include "shared.h"
#include "sync.h"
#include "text.h"

#include <fcntl.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    const char *name = "child2";
    pid_t parent_pid = getppid();
    int fd = -1;
    int result = EXIT_FAILURE;
    struct shared_block *block = NULL;
    int previous_space = 0;
    if (argc != 2) {
        fprintf(stderr, "Usage: %s SHARED_FILE\n", argv[0]);
        return EXIT_FAILURE;
    }
    fd = open(argv[1], O_RDWR);
    if (fd == -1) {
        perror("child2: open");
        goto cleanup;
    }
    if (shared_map(fd, &block) == -1) {
        perror("child2: shared_map");
        goto cleanup;
    }

    for (;;) {
        if (sync_wait(SIGUSR1, parent_pid, 0) == -1) {
            perror("child2: wait for permission");
            goto cleanup;
        }

        if (block->kind == MESSAGE_END) {
            if (kill(parent_pid, SIGUSR2) == -1) {
                perror("child2: acknowledge end");
                goto cleanup;
            }
            break;
        }
        if (block->kind != MESSAGE_DATA || block->length > SHARED_CAPACITY) {
            fprintf(stderr, "[%s] Invalid shared message\n", name);
            goto cleanup;
        }

        if (text_collapse_spaces(block, &previous_space) == -1) {
            perror("child2: text_collapse_spaces");
            goto cleanup;
        }
        if (fprintf(stderr, "[%s] PID=%ld, PPID=%ld, processed bytes=%zu\n",
                    name, (long)getpid(), (long)parent_pid, block->length) < 0 ||
            fflush(stderr) == EOF) {
            perror("child2: diagnostic output");
            goto cleanup;
        }
        if (kill(parent_pid, SIGUSR2) == -1) {
            perror("child2: send acknowledgement");
            goto cleanup;
        }
    }

    result = EXIT_SUCCESS;

cleanup:
    if (block != NULL && shared_unmap(block) == -1) {
        perror("child2: shared_unmap");
        result = EXIT_FAILURE;
    }
    if (fd != -1 && close(fd) == -1) {
        perror("child2: close");
        result = EXIT_FAILURE;
    }
    return result;
}
