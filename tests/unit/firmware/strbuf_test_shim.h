#ifndef STRBUF_TEST_SHIM_H
#define STRBUF_TEST_SHIM_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    size_t length;
    size_t max_len;
    uint8_t data_ptr_is_null;
} allocate_strbuf_result_t;

allocate_strbuf_result_t test_allocate_strbuf_and_append(const void *data, size_t length, uint8_t *out_data, size_t out_data_max);

#endif
