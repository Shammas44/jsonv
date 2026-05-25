#ifndef _jsonv_UTILS_H_INCLUDED
#define _jsonv_UTILS_H_INCLUDED
#include "except.h"
#include "stack.h"
#include "value.h"

#define JSONV_SCHEMA_IS_VALID -1

typedef struct {
  char description[100];
  const char *path; // Arena-allocated or zero-copy read-only view
  Jsonv_Except_Type type;
} E;

#endif
