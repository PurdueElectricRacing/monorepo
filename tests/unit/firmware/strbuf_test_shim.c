#include "strbuf_test_shim.h"

#include <string.h>

#include "strbuf.h"

allocate_strbuf_result_t test_allocate_strbuf_and_append(const void *data, size_t length, uint8_t *out_data, size_t out_data_max) {
    ALLOCATE_STRBUF(local, 16);

    strbuf_append(&local, data, length);

    allocate_strbuf_result_t result = {
        .length = local.length,
        .max_len = local.max_len,
        .data_ptr_is_null = (local.data == NULL),
    };

    size_t copy_len = local.length < out_data_max ? local.length : out_data_max;
    memcpy(out_data, local.data, copy_len);

    return result;
}
