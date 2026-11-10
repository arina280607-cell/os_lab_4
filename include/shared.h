#ifndef OS_LAB4_SHARED_H
#define OS_LAB4_SHARED_H

#include <stddef.h>

#define SHARED_CAPACITY 4096

enum message_kind {
    MESSAGE_DATA = 1,
    MESSAGE_END = 2
};

struct shared_block {
    size_t length;
    enum message_kind kind;
    char data[SHARED_CAPACITY];
};

int shared_map(int fd, struct shared_block **out);
int shared_unmap(struct shared_block *block);

#endif
