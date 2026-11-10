#include "shared.h"
#include "sync.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    const char *name = "child1";
    pid_t parent_pid = getppid();
    int fd = -1;
    int result = EXIT_FAILURE;
    struct shared_block *block = NULL;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s SHARED_FILE\n", argv[0]);
        return EXIT_FAILURE;
    }

    fd = open(argv[1], O_RDWR);
    if (fd == -1) {
        perror("child1: open");
        goto cleanup;
    }

    if (shared_map(fd, &block) == -1) {
        perror("child1: shared_map");
        goto cleanup;
    }

    if (sync_wait(SIGUSR1, parent_pid, 0) == -1) {
        perror("child1: wait for permission");
        goto cleanup;
    }

    if (block->kind != MESSAGE_DATA || block->length > SHARED_CAPACITY) {
        fprintf(stderr, "[%s] Invalid shared message\n", name);
        goto cleanup;
    }

    if (printf("[%s] PID=%ld, PPID=%ld, bytes=%zu\n",
               name, (long)getpid(), (long)parent_pid, block->length) < 0) {
        perror("child1: printf");
        goto cleanup;
    }
    if (fwrite(block->data, 1, block->length, stdout) != block->length) {
        perror("child1: fwrite");
        goto cleanup;
    }
    if (fputc('\n', stdout) == EOF) {
        perror("child1: fputc");
        goto cleanup;
    }
    if (fflush(stdout) == EOF) {
        perror("child1: fflush");
        goto cleanup;
    }

    if (kill(parent_pid, SIGUSR2) == -1) {
        perror("child1: send acknowledgement");
        goto cleanup;
    }

    result = EXIT_SUCCESS;

cleanup:
    if (block != NULL && shared_unmap(block) == -1) {
        perror("child1: shared_unmap");
        result = EXIT_FAILURE;
    }
    if (fd != -1 && close(fd) == -1) {
        perror("child1: close");
        result = EXIT_FAILURE;
    }

    return result;
}
