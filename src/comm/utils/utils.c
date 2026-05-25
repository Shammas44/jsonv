#include "utils.h"
#include <stdlib.h>
#include <string.h>

char *jsonv_substring_from_ptrs(const char *start, const char *end) {
  /*#region*/
  if (!start || !end || end < start) {
    return NULL;
  }

  size_t len = end - start;       // number of characters
  char *buffer = malloc(len + 1); // +1 for null terminator
  if (!buffer) {
    return NULL;
  }

  memcpy(buffer, start, len);
  buffer[len] = '\0';

  return buffer;
  /*#endregion*/
}

int jsonv_append_string_safe(char **dest_ptr, size_t *dest_len_ptr,
                             size_t *dest_cap_ptr, const char *src) {
  /*#region*/
  if (src == NULL)
    return 0;

  size_t src_len = strlen(src);
  // +1 for the null terminator. If we don't need the +1, we are at the limit.
  size_t required_len = *dest_len_ptr + src_len + 1;

  if (required_len > *dest_cap_ptr) {
    // Double the capacity, but ensure it's at least large enough for the
    // current operation
    size_t new_capacity = *dest_cap_ptr * 2;
    if (new_capacity < required_len) {
      new_capacity = required_len + (*dest_cap_ptr / 2); // Add a 50% buffer
    }

    char *new_dest = realloc(*dest_ptr, new_capacity);
    if (new_dest == NULL) {
      return -1; // Allocation failure
    }
    *dest_ptr = new_dest;
    *dest_cap_ptr = new_capacity;
  }

  // Append the string and update the length
  strcat(*dest_ptr, src);
  *dest_len_ptr += src_len;
  return 0;
  /*#endregion*/
}
