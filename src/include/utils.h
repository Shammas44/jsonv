#ifndef _JSONV_UTILS_H_INCLUDED
#define _JSONV_UTILS_H_INCLUDED
#include <sys/_types/_size_t.h>

#define JSON_INITIAL_BUFF_SIZE 4096
/**
 * Appends a source string (src) to a destination string (dest),
 * reallocating dest if necessary.
 *
 * @param dest_ptr      Pointer to the current buffer pointer (will be updated
 * on realloc).
 * @param dest_len_ptr  Pointer to the current length of the string in dest.
 * @param dest_cap_ptr  Pointer to the current allocated capacity of dest.
 * @param src           The string to append.
 * @return 0 on success, -1 on memory allocation failure.
 */
int jsonv_append_string_safe(char **dest_ptr, size_t *dest_len_ptr,
                             size_t *dest_cap_ptr, const char *src);

char *jsonv_substring_from_ptrs(const char *start, const char *end);
#endif
