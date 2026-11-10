#include "shared.h"

#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>

int shared_map(int fd, struct shared_block **out)
{
    if (out == NULL) {
        errno = EINVAL;
        return -1;
    }

    *out = NULL;
    struct stat info;

    if (fstat(fd, &info) == -1) {
        return -1;
    }

    if (info.st_size < 0 ||
        (size_t)info.st_size != sizeof(struct shared_block)) {
        errno = EINVAL;
        return -1;
    }

    void *address = mmap(NULL, sizeof(struct shared_block),
                         PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if (address == MAP_FAILED) {
        return -1;
    }

    *out = address;
    return 0;
}

int shared_unmap(struct shared_block *block)
{
    if (block == NULL) {
        errno = EINVAL;
        return -1;
    }

    return munmap(block, sizeof(struct shared_block));
}
