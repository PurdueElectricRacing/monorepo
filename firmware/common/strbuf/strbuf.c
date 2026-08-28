/**
 * @file strbuf.c
 * @brief Fixed-size string builder.
 * Useful for building large CMD strings before transmission.
 *
 * @author Irving Wang (irvingw@purdue.edu)
 * @author Daniel Proano (dproano@purdue.edu)
 */

#include "strbuf.h"
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

/**
 * @brief Clears the buffer by resetting length to 0.
 */
void strbuf_clear(strbuf_t *sb) {
    sb->length = 0;
}

/**
 * @brief Appends raw data to the buffer. If the data exceeds the buffer size, no data is appended.
 * @return Num bytes written to the buffer
 */
size_t strbuf_append(strbuf_t *sb, const void *data, size_t length) {
    if (sb->length + length > sb->max_len) {
        return 0;
    }

    uint8_t *buf_end_addr = sb->data + sb->length;
    memcpy(buf_end_addr, data, length);
    sb->length += length;

    return length;
}

/**
 * @brief Appends formatted data to the buffer using printf-style formatting.
 * ! @warning This function is unsafe if the formatted string exceeds the remaining buffer space.
 */
size_t strbuf_printf(strbuf_t *str_buf, const char *format, ...) {
    size_t remaining_space = str_buf->max_len - str_buf->length;

    if (remaining_space == 0) {
        return 0;
    } 

    va_list args;
    va_start(args, format);

    int len = vsnprintf(NULL, 0, format, args);
    va_end(args);

    if ((size_t)len >= remaining_space) {
        return 0;
    }

    va_start(args, format);
    char *buf_end = (char *)(str_buf->data + str_buf->length);
    vsnprintf(buf_end, remaining_space + 1, format, args);
    va_end(args);

    str_buf->length += (size_t)len;
    return (size_t)len;
}