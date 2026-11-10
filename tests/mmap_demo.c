#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

int main(void)
{
    const size_t mapping_size = 4096;
    char path[] = "/tmp/os_lab4_demo_XXXXXX";
    char ordinary[] = "linux";

    int fd = -1;
    int file_exists = 0;
    int result = EXIT_FAILURE;
    char *shared = MAP_FAILED;

    fd = mkstemp(path);

    if (fd == -1) {
        perror("mkstemp");
        goto cleanup;
    }

    file_exists = 1;

    if (unlink(path) == -1) {
        perror("unlink");
        goto cleanup;
    }

    file_exists = 0;

    if (ftruncate(fd, (off_t)mapping_size) == -1) {
        perror("ftruncate");
        goto cleanup;
    }

    shared = mmap(NULL,
                  mapping_size,
                  PROT_READ | PROT_WRITE,
                  MAP_SHARED,
                  fd,
                  0);

    if (shared == MAP_FAILED) {
        perror("mmap");
        goto cleanup;
    }

    memcpy(shared, ordinary, sizeof ordinary);

    pid_t child_pid = fork();

    if (child_pid == -1) {
        perror("fork");
        goto cleanup;
    }

    if (child_pid == 0) {
        ordinary[0] = 'L';
        shared[0] = 'L';

        _exit(EXIT_SUCCESS);
    }

    int status;

    while (waitpid(child_pid, &status, 0) == -1) {
        if (errno == EINTR) {
            continue;
        }

        perror("waitpid");
        goto cleanup;
    }

    if (!WIFEXITED(status) ||
        WEXITSTATUS(status) != EXIT_SUCCESS) {
        fprintf(stderr, "Child did not finish successfully\n");
        goto cleanup;
    }

    if (printf("[parent] PID=%ld, child PID=%ld\n"
               "ordinary: %s\n"
               "mapped:   %s\n",
               (long)getpid(),
               (long)child_pid,
               ordinary,
               shared) < 0) {
        perror("printf");
        goto cleanup;
    }

    if (fflush(stdout) == EOF) {
        perror("fflush");
        goto cleanup;
    }

    if (strcmp(ordinary, "linux") != 0 ||
        strcmp(shared, "Linux") != 0) {
        fprintf(stderr, "Unexpected memory contents\n");
        goto cleanup;
    }

    result = EXIT_SUCCESS;

cleanup:
    if (shared != MAP_FAILED) {
        if (munmap(shared, mapping_size) == -1) {
            perror("munmap");
            result = EXIT_FAILURE;
        }
    }

    if (fd != -1) {
        if (close(fd) == -1) {
            perror("close");
            result = EXIT_FAILURE;
        }
    }

    if (file_exists) {
        if (unlink(path) == -1) {
            perror("cleanup: unlink");
            result = EXIT_FAILURE;
        }
    }

    return result;
}