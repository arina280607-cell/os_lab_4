#include "text.h"

#include <errno.h>
#include <limits.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>

int text_uppercase(struct shared_block *block)
{
    if (block == NULL || block->length > SHARED_CAPACITY) {
        errno = EINVAL;
        return -1;
    }

    char output[SHARED_CAPACITY];
    size_t input_pos = 0;
    size_t output_pos = 0;
    mbstate_t input_state = {0};
    mbstate_t output_state = {0};

    while (input_pos < block->length) {
        wchar_t symbol;
        size_t consumed = mbrtowc(&symbol,
                                 block->data + input_pos,
                                 block->length - input_pos,
                                 &input_state);

        if (consumed == (size_t)-1 || consumed == (size_t)-2) {
            errno = EILSEQ;
            return -1;
        }
        if (consumed == 0) {
            consumed = 1;
        }

        wchar_t upper = (wchar_t)towupper((wint_t)symbol);
        char encoded[MB_LEN_MAX];
        size_t produced = wcrtomb(encoded, upper, &output_state);

        if (produced == (size_t)-1) {
            return -1;
        }
        if (produced > SHARED_CAPACITY - output_pos) {
            errno = EOVERFLOW;
            return -1;
        }

        memcpy(output + output_pos, encoded, produced);
        input_pos += consumed;
        output_pos += produced;
    }

    memcpy(block->data, output, output_pos);
    block->length = output_pos;
    return 0;
}

int text_collapse_spaces(struct shared_block *block, int *previous_space)
{
    if (block == NULL || previous_space == NULL ||
        block->length > SHARED_CAPACITY) {
        errno = EINVAL;
        return -1;
    }

    size_t output_pos = 0;

    for (size_t input_pos = 0; input_pos < block->length; ++input_pos) {
        char symbol = block->data[input_pos];

        if (symbol == ' ' && *previous_space) {
            continue;
        }

        block->data[output_pos++] = symbol;
        *previous_space = (symbol == ' ');
    }

    block->length = output_pos;
    return 0;
}
