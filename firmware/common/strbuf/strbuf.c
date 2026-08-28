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
 *
 * ! @warning Data is purely a buffer, not a C-string,
 *      meaning null terminators do not need to be considered
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
 * ! @warning The arguments provided may be C-strings and thus, 
 *      because of the internal use of vsnprintf, you might have 
 *      to account for a null terminator.
 */
size_t strbuf_printf(strbuf_t *sb, const char *format, ...) {
    size_t remaining_space = sb->max_len - sb->length;

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
    char *buf_end = (char *)(sb->data + sb->length);
    vsnprintf(buf_end, remaining_space + 1, format, args);
    va_end(args);

    sb->length += (size_t)len;
    return (size_t)len;
}