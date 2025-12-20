#include "error.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Initial capacity for dynamic arrays
#define INITIAL_CAPACITY 4

static bool jsonv_expand_array(char ***array, size_t *count, size_t *capacity,
                               size_t element_size) {
  /*#region*/
  if (*count < *capacity) {
    return true;
  }

  size_t new_capacity = (*capacity == 0) ? INITIAL_CAPACITY : *capacity * 2;
  char **new_array = (char **)realloc(*array, new_capacity * element_size);

  if (new_array == NULL) {
    return false; // Reallocation failed
  }

  *array = new_array;
  *capacity = new_capacity;
  return true;
  /*#endregion*/
}

/* ------------------------------
    JSONV PATH IMPLEMENTATION
    ------------------------------ */

bool jsonv_path_init(Jsonv_path *p) {
  /*#region*/
  p->segments = NULL;
  p->count = 0;
  p->capacity = 0;
  // Initial allocation will happen on first push, but you might want to force
  // it here:
  return jsonv_expand_array(&p->segments, &p->count, &p->capacity,
                            sizeof(char *));
  /*#endregion*/
}

void jsonv_path_free(Jsonv_path *p) {
  /*#region*/
  if (!p)
    return;
  // Free each segment string
  for (size_t i = 0; i < p->count; i++) {
    free(p->segments[i]);
  }
  // Free the array of segment pointers
  free(p->segments);
  p->segments = NULL;
  p->count = 0;
  p->capacity = 0;
  /*#endregion*/
}

void jsonv_path_reset(Jsonv_path *p) {
  /*#region*/
  // Free segment strings, but keep the underlying array allocated
  for (size_t i = 0; i < p->count; i++) {
    free(p->segments[i]);
    p->segments[i] = NULL;
  }
  p->count = 0;
  /*#endregion*/
}

static bool jsonv_path_push_string(Jsonv_path *p, const char *segment_data) {
  /*#region*/
  if (!jsonv_expand_array(&p->segments, &p->count, &p->capacity,
                          sizeof(char *))) {
    return false;
  }

  // Allocate space for the string and copy it
  char *new_segment = strdup(segment_data);
  if (new_segment == NULL) {
    return false;
  }

  p->segments[p->count] = new_segment;
  p->count++;
  return true;
  /*#endregion*/
}

bool jsonv_path_push(Jsonv_path *p, const char *segment) {
  /*#region*/
  // No length truncation needed here; strdup handles the length.
  // If you need to enforce a maximum length (e.g. JSONV_MAX_PATH_LEN),
  // you should check it before calling strdup.
  return jsonv_path_push_string(p, segment);
  /*#endregion*/
}

bool jsonv_path_push_index(Jsonv_path *p, int index) {
  /*#region*/
  char buffer[64]; // Temporary stack buffer for index formatting
  snprintf(buffer, sizeof(buffer), "[%d]", index);
  return jsonv_path_push_string(p, buffer);
  /*#endregion*/
}

void jsonv_path_pop(Jsonv_path *p) {
  /*#region*/
  if (p->count > 0) {
    p->count--;
    // Free the popped segment's string data
    free(p->segments[p->count]);
    p->segments[p->count] = NULL;
  }
  /*#endregion*/
}

char *jsonv_path_get(const Jsonv_path *p) {
  /*#region*/
  if (p->count == 0) {
    // Handle the case where no segments were ever pushed,
    // which might indicate an uninitialized path or an error condition.
    // Returning a placeholder is safer than returning NULL if the caller
    // expects a string.
    return strdup("$ERROR_EMPTY");
  }

  // 1. Calculate required length
  size_t total_len = 1; // Start with 1 for the null terminator '\0'

  // Calculate space needed for segments and separators
  for (size_t i = 0; i < p->count; i++) {
    total_len += strlen(p->segments[i]);

    // Add one byte for the '.' separator if:
    // 1. It is NOT the first segment (i > 0).
    // 2. The segment is NOT an array index (segment[0] != '[').
    if (i > 0 && p->segments[i][0] != '[') {
      total_len += 1; // Add space for '.'
    }
  }

  // 2. Allocate final path string on heap
  char *out = (char *)malloc(total_len);
  if (!out)
    return NULL;

  // 3. Build the string
  // Initialize the path with an empty string
  out[0] = '\0';

  for (size_t i = 0; i < p->count; i++) {
    const char *segment = p->segments[i];

    if (i == 0) {
      // Case 1: First segment (i=0). MUST be "$" or "@". Append directly.
      strcat(out, segment);

    } else if (segment[0] == '[') {
      // Case 2: Array index segment. Append directly, no prefix needed.
      strcat(out, segment);

    } else {
      // Case 3: Subsequent object key segment (i > 0). Requires a dot prefix.
      strcat(out, ".");
      strcat(out, segment);
    }
  }

  return out;
  /*#endregion*/
}

/* ------------------------------
    ERROR STACK IMPLEMENTATION
    ------------------------------ */

bool jsonv_error_init(Jsonv_error_stack *s) {
  /*#region*/
  s->messages = NULL;
  s->count = 0;
  s->capacity = 0;
  return jsonv_expand_array(&s->messages, &s->count, &s->capacity,
                            sizeof(char *));
  /*#endregion*/
}

void jsonv_error_free(Jsonv_error_stack *s) {
  /*#region*/
  if (!s)
    return;
  // Free each message string
  for (size_t i = 0; i < s->count; i++) {
    free(s->messages[i]);
  }
  // Free the array of message pointers
  free(s->messages);
  s->messages = NULL;
  s->count = 0;
  s->capacity = 0;
  /*#endregion*/
}

void jsonv_error_reset(Jsonv_error_stack *s) {
  /*#region*/
  // Free message strings, but keep the underlying array allocated
  for (size_t i = 0; i < s->count; i++) {
    free(s->messages[i]);
    s->messages[i] = NULL;
  }
  s->count = 0;
  /*#endregion*/
}

void jsonv_error_push(Jsonv_error_stack *s, const char *fmt, ...) {
  /*#region*/
  // The JSONV_ERR macro already handles the path formatting and strdup.
  // If called directly, fmt should be the heap-allocated message string.

  // 1. Check for heap allocation failure from macro
  if (s->messages == NULL && s->capacity > 0) {
    // Should not happen if init was successful, but good to check.
    return;
  }

  // 2. Expand array if needed
  if (!jsonv_expand_array(&s->messages, &s->count, &s->capacity,
                          sizeof(char *))) {
    // Failed to expand, skip message
    return;
  }

  // 3. Handle formatting for direct calls (less common, but required for the
  // interface) We assume the caller passed the HEAP-ALLOCATED string pointer
  // directly This is safer if you want to eliminate the internal vsnprintf:

  // Since the original signature allows varargs, we stick to it and use the
  // HEAP-ALLOCATED assumption only for the JSONV_ERR macro.

  // For general safety, we calculate the required size.
  va_list ap;
  va_start(ap, fmt);

  // Determine the required buffer size
  int len = vsnprintf(NULL, 0, fmt, ap);
  va_end(ap);

  if (len < 0)
    return; // Encoding error

  // Allocate buffer for the final message (including null terminator)
  char *new_msg = (char *)malloc(len + 1);
  if (!new_msg)
    return;

  // Format the message into the heap buffer
  va_start(ap, fmt);
  vsnprintf(new_msg, len + 1, fmt, ap);
  va_end(ap);

  // Store the heap-allocated message and increment count
  s->messages[s->count] = new_msg;
  s->count++;
  /*#endregion*/
}

void jsonv_error_print(const Jsonv_error_stack *s) {
  /*#region*/
  for (size_t i = 0; i < s->count; i++) {
    printf("Error %zu: %s\n", i + 1, s->messages[i]);
  }
  /*#endregion*/
}
