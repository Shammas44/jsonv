#ifndef _JSONV_PRESCAN_H_INCLUDED
#define _JSONV_PRESCAN_H_INCLUDED
#include <stdbool.h>
#include <stdio.h>

typedef struct {
  size_t max_depth;
  size_t cur_depth;
  size_t value_count;
  size_t object_count;
  size_t array_count;
  size_t string_bytes;
} JsonEstimate;

void prescan(const char *s, size_t len, JsonEstimate *out);

#endif
