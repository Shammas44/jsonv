#include "error.h"
#include <stdio.h>
#include <string.h>

/* ------------------------------
   JSONV PATH IMPLEMENTATION
   ------------------------------ */

void jsonv_path_reset(Jsonv_path *p) {
  /*#region*/
  p->count = 0;
  /*#endregion*/
}

void jsonv_path_push(Jsonv_path *p, const char *segment) {
  /*#region*/
  if (p->count < JSONV_MAX_PATH_SEG) {
    strncpy(p->segments[p->count], segment, JSONV_MAX_PATH_LEN - 1);
    p->segments[p->count][JSONV_MAX_PATH_LEN - 1] = 0;
    p->count++;
  }
  /*#endregion*/
}

void jsonv_path_push_index(Jsonv_path *p, int index) {
  /*#region*/
  if (p->count < JSONV_MAX_PATH_SEG) {
    snprintf(p->segments[p->count], JSONV_MAX_PATH_LEN, "[%d]", index);
    p->count++;
  }
  /*#endregion*/
}

void jsonv_path_pop(Jsonv_path *p) {
  /*#region*/
  if (p->count > 0)
    p->count--;
  /*#endregion*/
}

void jsonv_path_get(const Jsonv_path *p, char out[JSONV_MAX_PATH_LEN]) {
  /*#region*/
  out[0] = 0;
  for (int i = 0; i < p->count; i++) {
    if (p->segments[i][0] == '[')
      strncat(out, p->segments[i], JSONV_MAX_PATH_LEN - strlen(out) - 1);
    else {
      if (i > 0)
        strncat(out, ".", JSONV_MAX_PATH_LEN - strlen(out) - 1);
      strncat(out, p->segments[i], JSONV_MAX_PATH_LEN - strlen(out) - 1);
    }
  }
  /*#endregion*/
}

/* ------------------------------
   ERROR STACK IMPLEMENTATION
   ------------------------------ */

void jsonv_error_reset(Jsonv_error_stack *s) {
  /*#region*/
  s->count = 0;
  /*#endregion*/
}

void jsonv_error_push(Jsonv_error_stack *s, const char *fmt, ...) {
  /*#region*/
  if (s->count >= JSONV_MAX_ERROR_STACK)
    return;

  va_list ap;
  va_start(ap, fmt);
  vsnprintf(s->messages[s->count], JSONV_MAX_ERROR_MSG, fmt, ap);
  va_end(ap);

  s->count++;
  /*#endregion*/
}

void jsonv_error_print(const Jsonv_error_stack *s) {
  /*#region*/
  for (int i = 0; i < s->count; i++)
    printf("Error %d: %s\n", i + 1, s->messages[i]);
  /*#endregion*/
}
