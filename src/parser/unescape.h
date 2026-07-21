#ifndef _JSONV_UNESCAPE_H
#define _JSONV_UNESCAPE_H

#include <stddef.h>

/**
 * @brief Unescapes a JSON UTF-8 string slice into a destination buffer.
 *
 * Handles standard single-character escapes (\", \\, \/, \b, \f, \n, \r, \t)
 * and UTF-16 surrogate pairs (\uD800..\uDBFF + \uDC00..\uDFFF) encoded into UTF-8.
 *
 * @param src Source JSON string slice (without surrounding quotes)
 * @param len Length of the source slice
 * @param dest Output buffer (must be at least len + 1 bytes)
 * @return Number of unescaped bytes written to dest (excluding null terminator)
 */
size_t jsonv_unescape_string(const unsigned char *src, size_t len, char *dest);

#endif
