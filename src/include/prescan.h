#ifndef _JSONV_PRESCAN_H_INCLUDED
#define _JSONV_PRESCAN_H_INCLUDED
#include <stdbool.h>
#include <stdio.h>

typedef struct {
  int max_depth;
  int cur_depth;
  size_t value_count;
  size_t object_count;
  size_t array_count;
  size_t string_bytes;
} JsonEstimate;

void jsonv_prescan(const char *s, size_t len, JsonEstimate *out);

#endif
