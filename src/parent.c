#include "shared.h"
#include "sync.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

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

int main(void)
{
    char shared_path[] = "/tmp/os_lab4_XXXXXX";
    int fd = -1;
    int file_exists = 0;
    int result = EXIT_FAILURE;
    struct shared_block *block = NULL;
    pid_t child1_pid = -1;
    pid_t child2_pid = -1;

    if (sync_prepare() == -1) {
        perror("parent: sync_prepare");
        goto cleanup;
    }

    fd = mkstemp(shared_path);
    if (fd == -1) {
        perror("parent: mkstemp");
        goto cleanup;
    }
    file_exists = 1;

    if (ftruncate(fd, (off_t)sizeof(struct shared_block)) == -1) {
        perror("parent: ftruncate");
        goto cleanup;
    }

    if (shared_map(fd, &block) == -1) {
        perror("parent: shared_map");
        goto cleanup;
    }

    const char text[] = "hello   world";
    block->length = sizeof text - 1;
    block->kind = MESSAGE_DATA;
    memcpy(block->data, text, block->length);

    if (printf("[parent] PID=%ld, shared file: %s\n",
               (long)getpid(), shared_path) < 0) {
        perror("parent: printf");
        goto cleanup;
    }
    if (fflush(stdout) == EOF) {
        perror("parent: fflush");
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

    if (run_stage(child1_pid) == -1 || run_stage(child2_pid) == -1) {
        goto cleanup;
    }

    result = EXIT_SUCCESS;

cleanup:
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

    return result;
}
