#ifndef OS_LAB4_TEXT_H
#define OS_LAB4_TEXT_H

#include "shared.h"

int text_uppercase(struct shared_block *block);
int text_collapse_spaces(struct shared_block *block, int *previous_space);

#endif
